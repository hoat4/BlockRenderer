# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(BlockRenderer C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

#add_executable(BlockRenderer BlockRenderer.cpp dvhstx.cpp dvi.cpp intel_one_mono_2bpp.c)
add_executable(BlockRenderer 
    BlockRenderer.cpp DVIDisplay.cpp Rasterizer.cpp TriangleDemo.cpp Cubes.cpp Fill.cpp)

pico_set_program_name(BlockRenderer "BlockRenderer")
pico_set_program_version(BlockRenderer "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(BlockRenderer 1)
pico_enable_stdio_usb(BlockRenderer 0)

# Add the standard library to the build
target_link_libraries(BlockRenderer
        pico_stdlib hardware_dma hardware_interp pico_rand)

# Add the standard include files to the build
target_include_directories(BlockRenderer PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

#target_compile_options(BlockRenderer PRIVATE -O0)

pico_add_extra_outputs(BlockRenderer)

