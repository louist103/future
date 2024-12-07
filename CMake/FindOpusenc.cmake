# FindOpusenc.cmake
# This module locates the opusenc library and its headers.
# It defines the following variables:
#  - Opusenc_FOUND: Boolean indicating if opusenc was found
#  - Opusenc_INCLUDE_DIRS: Include directories for opusenc
#  - Opusenc_LIBRARIES: Libraries to link against opusenc
#  - Opusenc_VERSION: Version of the found opusenc library (optional)

# Locate the opusenc header
find_path(Opusenc_INCLUDE_DIR
    NAMES opusenc.h
    HINTS
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include
        opus
)

# Locate the opusenc library
find_library(Opusenc_LIBRARY
    NAMES opusenc
    HINTS
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        lib
)

# Check if both the library and headers were found
if (Opusenc_INCLUDE_DIR AND Opusenc_LIBRARY)
    set(Opusenc_FOUND TRUE)
else ()
    set(Opusenc_FOUND FALSE)
endif ()

# Provide include directories and libraries if found
if (Opusenc_FOUND)
    set(Opusenc_INCLUDE_DIRS ${Opusenc_INCLUDE_DIR})
    set(Opusenc_LIBRARIES ${Opusenc_LIBRARY})
    if(NOT TARGET Opus::opusenc)
    add_library(Opus::opusenc UNKNOWN IMPORTED)
        set_target_properties(Opus::opusenc PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Opusenc_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${Opusenc_LIBRARIES}"
        )
    endif()
    # Optional: Retrieve library version
    set(Opusenc_VERSION "Unknown") # Adjust if version detection is needed
endif ()

# Mark variables as advanced to reduce clutter
mark_as_advanced(Opusenc_INCLUDE_DIR Opusenc_LIBRARY)
