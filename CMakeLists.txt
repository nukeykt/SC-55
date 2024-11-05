cmake_minimum_required (VERSION 3.19)
project (NukedSC55 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#==============================================================================
# Tests
#==============================================================================
option(NUKED_ENABLE_TESTS "Enable tests" OFF)
set(NUKED_TEST_ROMDIR "" CACHE PATH
    "Directory the test runner should look for roms in")

if(NUKED_ENABLE_TESTS)
    enable_testing()
    add_subdirectory("test")
endif()

#==============================================================================
# Dependencies
#==============================================================================
find_package(SDL2 REQUIRED)

if(NOT WIN32)
    set(USE_RTMIDI TRUE)
endif()

if(USE_RTMIDI)
    find_package(rtmidi MODULE REQUIRED)
endif()

#==============================================================================
# Support functions
#==============================================================================
function(target_enable_warnings tgt)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4
            # Disable the following warnings to match clang/gcc
            /wd4018 # 'expression': signed/unsigned mismatch
            /wd4389 # 'operator': signed/unsigned mismatch
            /wd4244 # 'conversion_type': conversion from 'type1' to 'type2', possible loss of data
            /wd4267 # 'variable': conversion from 'size_t' to 'type', possible loss of data
        )
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -pedantic -Wshadow)
    endif()
endfunction()

function(target_enable_conversion_warnings tgt)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE
            /w44018 # 'expression': signed/unsigned mismatch
            /w44389 # 'operator': signed/unsigned mismatch
            /w44244 # 'conversion_type': conversion from 'type1' to 'type2', possible loss of data
            /w44267 # 'variable': conversion from 'size_t' to 'type', possible loss of data
        )
    else()
        target_compile_options(${tgt} PRIVATE -Wconversion)
    endif()
endfunction()

#==============================================================================
# Backend
#==============================================================================
add_library(nuked-sc55-backend)
target_sources(nuked-sc55-backend
    PRIVATE
    src/mcu.cpp src/mcu.h
    src/lcd.cpp src/lcd.h src/lcd_font.h
    src/mcu_interrupt.cpp src/mcu_interrupt.h
    src/mcu_opcodes.cpp src/mcu_opcodes.h
    src/mcu_timer.cpp src/mcu_timer.h
    src/pcm.cpp src/pcm.h
    src/submcu.cpp src/submcu.h
    src/emu.cpp src/emu.h
    src/ringbuffer.h
    src/math_util.h
    src/path_util.cpp src/path_util.h
    src/command_line.h
    src/cast.h
)
target_include_directories(nuked-sc55-backend PUBLIC "src")
target_link_libraries(nuked-sc55-backend PRIVATE SDL2::SDL2)
target_compile_features(nuked-sc55-backend PRIVATE cxx_std_23)
target_enable_warnings(nuked-sc55-backend)

#==============================================================================
# Main Frontend
#==============================================================================
add_executable(nuked-sc55)
target_sources(nuked-sc55
    PRIVATE
    src/midi.h
    src/main_frontend.cpp # main() is here!
)

if(USE_RTMIDI)
    target_sources(nuked-sc55 PRIVATE src/midi_rtmidi.cpp)
elseif(WIN32)
    target_sources(nuked-sc55 PRIVATE src/midi_win32.cpp)
endif()

target_link_libraries(nuked-sc55 PRIVATE SDL2::SDL2 SDL2::SDL2main nuked-sc55-backend)
target_compile_features(nuked-sc55 PRIVATE cxx_std_23)
target_enable_warnings(nuked-sc55)
target_enable_conversion_warnings(nuked-sc55)

if(USE_RTMIDI)
    target_link_libraries(nuked-sc55 PRIVATE RtMidi::rtmidi)
endif()

if(WIN32)
    target_link_libraries(nuked-sc55 PRIVATE shlwapi winmm)
endif()

if(APPLE)
    find_library(LIBCoreAudio CoreAudio)
    target_link_libraries(nuked-sc55 PRIVATE ${LIBCoreAudio})
endif()

#==============================================================================
# Renderer Frontend
#==============================================================================
add_executable(nuked-sc55-render)
target_sources(nuked-sc55-render
    PRIVATE
    src/render_frontend.cpp
    src/smf.cpp src/smf.h
    src/wav.cpp src/wav.h
)

target_link_libraries(nuked-sc55-render PRIVATE nuked-sc55-backend)
target_compile_features(nuked-sc55-render PRIVATE cxx_std_23)
target_enable_warnings(nuked-sc55-render)
target_enable_conversion_warnings(nuked-sc55-render)

#==============================================================================
# Installables
#==============================================================================
include(GNUInstallDirs)
install(TARGETS nuked-sc55 nuked-sc55-render)
install(FILES data/back.data DESTINATION ${CMAKE_INSTALL_DATADIR}/nuked-sc55)
