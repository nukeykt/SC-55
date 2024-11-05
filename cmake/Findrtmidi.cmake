# First try to use a cmake build if there is one
find_package(rtmidi CONFIG)

if(${rtmidi_FOUND})

    # The rtmidi find_package config wants us to #include <rtmidi/RtMidi.h>
    # which is different from how documentation and pkg-config does it.
    get_target_property(RTMIDI_INCLUDE_DIR RtMidi::rtmidi INTERFACE_INCLUDE_DIRECTORIES)
    string(APPEND RTMIDI_INCLUDE_DIR "/rtmidi")

    set_target_properties(RtMidi::rtmidi PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${RTMIDI_INCLUDE_DIR}"
    )

else()

    # TODO: attempt to pkg-config?
    find_path(RTMIDI_INCLUDE_DIR RtMidi.h PATH_SUFFIXES rtmidi)

    find_library(RTMIDI_LIBRARY NAMES rtmidi)

    add_library(RtMidi::rtmidi INTERFACE IMPORTED)
    set_target_properties(RtMidi::rtmidi PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${RTMIDI_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${RTMIDI_LIBRARY}"
    )

endif()

