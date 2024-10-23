cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(trunc-cpp CXX)

find_package(PkgConfig)
pkg_check_modules(MPFR REQUIRED mpfr gmp)

find_package(Enzyme REQUIRED CONFIG)

add_executable(enzyme src/main_enzyme.cpp)
target_compile_definitions(enzyme PUBLIC -DNOVERSION)
target_compile_options(enzyme PUBLIC "-include" "enzyme/fprt/mpfr.h")
target_link_libraries(enzyme PUBLIC LLDEnzymeFlags ${MPFR_LINK_LIBRARIES})
target_include_directories(enzyme PUBLIC ${MPFR_INCLUDE_DIRS} "/scratch/fhrold/riken/Enzyme/enzyme/include")

add_executable(normal src/main_normal.cpp)
