// build with 'cmake -DCMAKE_PREFIX_PATH="${HDF5_ROOT};/home/debian/examples/HighFive/build/install" -B build .'
// compile with 'cmake --build build --verbose'

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <sys/time.h>
#include <cmath>
#include <highfive/H5Easy.hpp>
using namespace std;

// Physical constants
#define VRef 4.096
#define sampRate 384

// PRU defined constants
#define averaging 8
// Memory address bases
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)
off_t SHARED_MEM = 0x4a310000;
off_t PRU0_MEM = 0x4a300000;
off_t PRU1_MEM = 0x4a302000;
// Memory buffers and allocated space
const int wordsPerBuf = 200;
#define BUFF_SIZE (24*wordsPerBuf)

// GPIO pins
const int pps_pin = 48;

// PRU firmware
const string PRU0_PATH = "pru/pru_readData.out";
const string PRU1_PATH = "pru/pru1.out";

// Time
const int time_len = 10;

// SD card path
const string SD_PATH = "/home/debian/examples/SDcard/";

// HDF5 files
const int bufsToFile = 10;
const int dataLen = bufsToFile*wordsPerBuf;

// Reads current value of gpio pin
int readGPIO(int gpio) {
    char filename[32];
    FILE *fp;
    int v;
    
    snprintf(filename,sizeof(filename),"/sys/class/gpio/gpio%d/value",gpio);
    fp = fopen(filename,"r");
    fscanf(fp,"%d",(int *)&v);
    fclose(fp);
    return (v);
} //readGPIO

// Return two's complement for negative binary numbers
uint32_t twos_comp(uint32_t val) {
    val = ~val + 1;
    return(val & ((1<<24) - 1)); // preserve only first 24 LSBs (2^25 - 1)
} // twos_comp(val)

// Convert digital read from ADC and convert to a voltage
double get_voltage(int binary) {
    if(binary & (1<<23)) {
        binary = twos_comp(binary);
        return((double)binary / (double)8388608 * VRef * -1);
    } else {
        return((double)binary / (double)8388607 * VRef);
    }

} //get_voltage


int main(void) {
    // ---- initial memory stuff ----
    int fd;
    void *map_base, *virt_addr;
    if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        printf("Failed to open memory\n");
        return -1;
    }
    fflush(stdout);
    // ---- initial memory stuff ----

    // pointers for shared memory
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SHARED_MEM& ~MAP_MASK);
    if(map_base == (void *) -1) {
        printf("Failed to map base address\n");
        return -1;
    }
    fflush(stdout);
    virt_addr = map_base + (SHARED_MEM & MAP_MASK);
    uint8_t *firstBuf_base = (uint8_t *) virt_addr;
    uint8_t *secondBuf_base = firstBuf_base + BUFF_SIZE / 8;
    uint8_t *wordPtr = firstBuf_base;
    // pointers for pru0 memory
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PRU0_MEM& ~MAP_MASK);
    if(map_base == (void *) -1) {
        printf("Failed to map base address\n");
        return -1;
    }
    fflush(stdout);
    virt_addr = map_base + (PRU0_MEM & MAP_MASK);
    uint8_t *pru0Data_base = (uint8_t *) virt_addr;
    uint8_t *pru0Rdy_base = pru0Data_base + 1;
    // pointers for pru1 memory
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PRU1_MEM& ~MAP_MASK);
    if(map_base == (void *) -1) {
        printf("Failed to map base address\n");
        return -1;
    }
    fflush(stdout);
    virt_addr = map_base + (PRU1_MEM & MAP_MASK);
    uint32_t *pru1Time_base = (uint32_t *) virt_addr;

    // SD card business
    if(system(("ls " + SD_PATH).c_str()) != 0) {
        system(("mkdir " + SD_PATH).c_str());
    } // create path if doesn't exist
    system(("mount /dev/mmcblk0p1 " + SD_PATH).c_str());

    // HDF5 file
    H5Easy::File file(SD_PATH + "Site_A.h5", H5Easy::File::Overwrite);
    vector<vector<double>> h5Data(2, vector<double>(dataLen));
    int vecIndex = 0;
    int dSetCount = 0;

    // buffer vars
    int bufReady = *pru0Data_base;
    uint32_t buf[BUFF_SIZE / 24];
    // pps vars
    int pps = readGPIO(pps_pin);
    int last_pps = pps;
    // time keeping
    double times[time_len] = {0};
    double dt;
    timeval tv;
    double time_base;
    // iterators
    int i; // general use
    int j = 0; // time keeping

    // load PRU firmware
    ofstream PRU_file;
    PRU_file.open("/sys/class/remoteproc/remoteproc1/state");
    PRU_file << "stop";
    PRU_file.close();
    PRU_file.open("/sys/class/remoteproc/remoteproc2/state");
    PRU_file << "stop";
    PRU_file.close();
    PRU_file.open("/sys/class/remoteproc/remoteproc1/firmware");
    PRU_file << PRU0_PATH;
    PRU_file.close();
    PRU_file.open("/sys/class/remoteproc/remoteproc2/firmware");
    PRU_file << PRU1_PATH;
    PRU_file.close();
    
    // wait for pps
    while(pps == 0) {pps = readGPIO(pps_pin);}

    // populate time samples and start PRUs
    *pru1Time_base = 0;
    *pru0Rdy_base = 1;
    pps = readGPIO(pps_pin);
    last_pps = pps;
    PRU_file.open("/sys/class/remoteproc/remoteproc2/state");
    PRU_file << "start";
    PRU_file.close();
    PRU_file.open("/sys/class/remoteproc/remoteproc1/state");
    PRU_file << "start";
    PRU_file.close();
    while(times[time_len - 1] == 0) {
        // Checks for rising edge of PPS
        pps = readGPIO(pps_pin);
        if(last_pps == 0 && pps == 1) {
            times[j] = 1.0 / *pru1Time_base;
            j++;
            j %= time_len;
            dt = 0;
            for(i=0; i<time_len; i++) {
                dt += times[i];
            } // for
            dt *= sampRate * averaging;
            dt /= time_len;
            printf("%.15f\n", dt);
            
        } // if
        last_pps = pps;
    } // while (populate time samples)

    // wait for low then high pps and begin sampling
    PRU_file.open("/sys/class/remoteproc/remoteproc2/state");
    PRU_file << "stop";
    PRU_file.close();
    *pru0Rdy_base = 0;
    while(pps == 1) {pps = readGPIO(pps_pin);}
    while(pps == 0) {pps = readGPIO(pps_pin);}
    gettimeofday(&tv, NULL);
    time_base = (double) round((double) tv.tv_sec + ((double) tv.tv_usec / 1000000));
    printf("%.6f\n", time_base);
    PRU_file.open("/sys/class/remoteproc/remoteproc2/state");
    PRU_file << "start";
    PRU_file.close();

    pps = readGPIO(pps_pin);
    last_pps = pps;
    // ---- loop forever ----
    while(1) {

        // Checks if data from PRU0 is ready
        if(bufReady != *pru0Data_base) {
            bufReady = *pru0Data_base;
            if(bufReady) {wordPtr = firstBuf_base;}
            else         {wordPtr = secondBuf_base;}
            for(i=0; i<BUFF_SIZE / 24; i++) {
                buf[i] = (*(wordPtr + 2) << 16) | (*(wordPtr + 1) << 8) | *wordPtr;
                wordPtr += 3;
                h5Data[0][vecIndex] = time_base;
                h5Data[1][vecIndex] = get_voltage(buf[i]);
                time_base += dt;
                vecIndex++;
            } // for
            if(vecIndex >= dataLen) {
                vecIndex = 0;
                H5Easy::dump(file, "dSet_" + to_string(dSetCount), h5Data);
                dSetCount++;
            } // if
        } // if

        // Checks for rising edge of PPS
        pps = readGPIO(pps_pin);
        if(last_pps == 0 && pps == 1) {
            times[j] = 1.0 / *pru1Time_base;
            j++;
            j %= time_len;
            dt = 0;
            for(i=0; i<time_len; i++) {
                dt += times[i];
            } // for
            dt *= sampRate * averaging;
            dt /= time_len;
            printf("%.15f\n", dt);
        } // if
        last_pps = pps;

    } // loop forever

}   // main