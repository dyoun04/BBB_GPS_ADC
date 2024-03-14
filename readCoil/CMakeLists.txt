cmake_minimum_required(VERSION 3.19)
project(UseHighFive)

find_package(HighFive REQUIRED)
add_executable(readCoil readCoil.cpp)
target_link_libraries(readCoil HighFive)