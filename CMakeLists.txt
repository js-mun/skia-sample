cmake_minimum_required(VERSION 3.15)
project(SkiaSample)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(SKIA_DIR "$ENV{HOME}/work/skia/skia")
set(SKIA_OUT "$ENV{HOME}/work/skia/out/vulkan")

find_package(Freetype REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

include_directories(
    ${SKIA_DIR}
    ${SKIA_OUT}/gen
    ${FREETYPE_INCLUDE_DIRS}
    ${glfw3_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

add_executable(sample main.cpp)

target_link_libraries(sample
    ${SKIA_OUT}/libskia.a
    pthread
    dl
    m
    vulkan
    fontconfig
    ${FREETYPE_LIBRARIES}
    ${glfw3_LIBRARIES}
    ${Vulkan_LIBRARIES}
)
