# SHITFIRE engine port — engine-free sim core + parity tests.
# See port/README.md for version pins, build commands, and porting rules.
cmake_minimum_required(VERSION 3.21)
project(shitfire_port LANGUAGES CXX)

enable_testing()

add_subdirectory(core)
add_subdirectory(core-tests)
