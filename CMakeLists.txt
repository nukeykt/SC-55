cmake_minimum_required (VERSION 3.2...3.5)
project (NukedSC55 VERSION 0.2.0 LANGUAGES C CXX)

include(FindPkgConfig)
include(GNUInstallDirs)
include(CheckCXXCompilerFlag)
include(CheckFunctionExists)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Prefer C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

#===========================================================================================
# Strip garbage
if(APPLE)
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")
elseif(NOT MSVC AND NOT MSDOS AND NOT OPENBSD_LOCALBASE)
    string(REGEX REPLACE "-O3" ""
            CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "-O3" ""
            CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fdata-sections -ffunction-sections")
    if(ANDROID)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -funwind-tables")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -funwind-tables")
    endif()
    if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -Wl,--gc-sections -Wl,-s")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -Wl,--gc-sections -Wl,-s")
        set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
    endif()
endif()

if(NOT MSVC AND NOT MSDOS)
# Global optimization flags
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
# Turn on all warnings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
# Deny undefined symbols
    if(NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined" )
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined" )
    endif()
# Supress the std::vector::insert() GCC change warning
    if(VITA)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVITA -DVITA=1 -fcompare-debug-second")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVITA -DVITA=1 -fcompare-debug-second")
    endif()
    if(NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_SWITCH)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcompare-debug-second")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcompare-debug-second")
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if(NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_SWITCH)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu90")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c89")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98")
endif()

if(DEFINED CMAKE_BUILD_TYPE)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
endif()
if(CMAKE_BUILD_TYPE_LOWER EQUAL "release")
    add_definitions(-DNDEBUG)
ENDIF()

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    add_definitions(-DDEBUG_BUILD)
    if(CMAKE_COMPILER_IS_GNUCXX)
        add_definitions(-D_GLIBCXX_DEBUG=1 -D_GLIBCXX_ASSERTIONS=1)
    endif()
endif()

# Disable bogus MSVC warnings
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(OPENBSD_LOCALBASE)
    message("OpenBSD Detected!")
    include_directories(${OPENBSD_LOCALBASE}/include)
endif()

if(NOT WIN32 AND CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    check_cxx_compiler_flag("-no-pie" HAS_NO_PIE)
endif()

function(set_nopie _target)
    if(NOT WIN32 AND HAS_NO_PIE AND CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
    endif()
endfunction()

function(set_legacy_standard destTarget)
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        # Turn on warnings and legacy C/C++ standards to support more compilers
        target_compile_options(${destTarget} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Wall -pedantic -std=gnu90 -Wno-long-long>
            $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic -std=gnu++98>
        )
    endif()
endfunction()

function(set_visibility_hidden destTarget)
    if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND NOT DJGPP)
        if(CMAKE_VERSION VERSION_GREATER 3.2)
            target_compile_options(${destTarget} PRIVATE
              $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden> -fvisibility=hidden)
        else()
            target_compile_options(${destTarget} PRIVATE -fvisibility=hidden)
        endif()
        target_compile_definitions(${destTarget} PRIVATE LIBADLMIDI_VISIBILITY=)
    endif()
endfunction()
#===========================================================================================

if(NOT WIN32 AND NOT NINTENDO_SWITCH AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX))
    check_cxx_compiler_flag("-no-pie" HAS_NO_PIE)
endif()

function(nuked-sc55_set_nopie _target)
    set_target_properties(${_target} PROPERTIES
        POSITION_INDEPENDENT_CODE False
    )
    if(HAS_NO_PIE AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX))
        set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
    endif()
endfunction()

function(nuked-sc55_find_SDL2)
    if(NOT TARGET ADLMIDI_SDL2)
        find_package(SDL2 REQUIRED)
        add_library(ADLMIDI_SDL2 INTERFACE)
        if(TARGET SDL2::SDL2)
            target_link_libraries(ADLMIDI_SDL2 INTERFACE SDL2::SDL2)
        else()
            string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
            target_include_directories(ADLMIDI_SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(ADLMIDI_SDL2 INTERFACE ${SDL2_LIBRARIES})
        endif()
    endif()
endfunction()

#===========================================================================================

if(NOT WIN32)
    check_function_exists(fstatat FILES_HAS_REALPATH)
    if(FILES_HAS_REALPATH)
        add_definitions(-DFILES_HAS_REALPATH)
    endif()
endif()

include(3rdparty/Utf8Main/utf8main.cmake)

if(NOT WIN32 AND PKG_CONFIG_FOUND)
    set(USE_RTMIDI TRUE)
    option(USE_SYSTEM_RTMIDI "Use system libraries for RtMidi" OFF)
endif()

find_package(SDL2 REQUIRED)

if(USE_RTMIDI)
    if(USE_SYSTEM_RTMIDI)
        pkg_check_modules(SYSTEM_RTMIDI rtmidi REQUIRED)
    else()
        include(RtMidi)
    endif()
endif()


set(SC55_SRC
    src/mcu.cpp src/mcu.h # main() is here!
    src/lcd.cpp src/lcd.h src/lcd_font.h
    src/mcu_interrupt.cpp src/mcu_interrupt.h
    src/mcu_opcodes.cpp src/mcu_opcodes.h
    src/mcu_timer.cpp src/mcu_timer.h
    src/midi.h
    src/serial.h
    src/pcm.cpp src/pcm.h
    src/submcu.cpp src/submcu.h

    src/utils/files.cpp src/utils/files.h
)

if(USE_RTMIDI)
    list(APPEND SC55_SRC src/midi_rtmidi.cpp)
elseif(WIN32)
    list(APPEND SC55_SRC src/midi_win32.cpp)
endif()

if(WIN32)
    list(APPEND SC55_SRC src/serial_win32.c)
else()
    list(APPEND SC55_SRC src/serial_dummy.c)
endif()

add_executable(nuked-sc55 ${SC55_SRC} ${UTF8MAIN_SRCS})
set_nopie(nuked-sc55)

if(MSVC)
    target_compile_definitions(nuked-sc55 PRIVATE WIN32_CONSOLE)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(nuked-sc55 PRIVATE SDL2::SDL2)
else()
    string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
    target_include_directories(nuked-sc55 PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(nuked-sc55 PRIVATE ${SDL2_LIBRARIES})
endif()

if(USE_RTMIDI)
    if(USE_SYSTEM_RTMIDI)
        target_compile_options(nuked-sc55 PRIVATE ${SYSTEM_RTMIDI_CFLAGS})
        target_include_directories(nuked-sc55 PRIVATE ${SYSTEM_RTMIDI_INCLUDE_DIRS})
        target_link_libraries(nuked-sc55 PRIVATE ${SYSTEM_RTMIDI_LIBRARIES})
    else()
        target_link_libraries(nuked-sc55 PRIVATE RtMidi)
    endif()
endif()

if(WIN32)
    target_link_libraries(nuked-sc55 PRIVATE shlwapi winmm)
endif()

if(APPLE)
    find_library(LIBCoreAudio CoreAudio)
    target_link_libraries(nuked-sc55 PRIVATE ${LIBCoreAudio})
endif()


set(SC55_INSTALL_FILES)

# Copy ROM files into runtime directory
macro(copy_rom_target TargetName TargetFile)
    add_custom_target(sc55_copy_${TargetName} ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${TargetFile}")
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${TargetFile}"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/data/${TargetFile}"
                "${CMAKE_CURRENT_BINARY_DIR}/${TargetFile}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/${TargetFile}"
    )
    list(APPEND SC55_INSTALL_FILES ${CMAKE_CURRENT_BINARY_DIR}/${TargetFile})
endmacro()

copy_rom_target(back_data back.data)
copy_rom_target(sc55mkII_background.bmp sc55mkII_background.bmp)

install(TARGETS nuked-sc55
        EXPORT NukedSC55StaticTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(FILES ${SC55_INSTALL_FILES}
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/nuked-sc55")
