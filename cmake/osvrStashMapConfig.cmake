# Manipulate CMAKE_MAP_IMPORTED_CONFIG_ cautiously and reversibly.
macro(osvr_stash_map_config config)
    # Re-entrancy protection
    list(APPEND OSVR_IN_MAP_CONFIG_STASH_${config} yes)
    list(LENGTH OSVR_IN_MAP_CONFIG_STASH_${config} OSVR_IN_MAP_CONFIG_STASH_LEN)
    if(OSVR_IN_MAP_CONFIG_STASH_LEN GREATER 1)
        # Not the first stash, get out.
        return()
    endif()

    # Actually perform the saving and replacement of CMAKE_MAP_IMPORTED_CONFIG_${config}
    if(DEFINED CMAKE_MAP_IMPORTED_CONFIG_${config})
        set(OSVR_OLD_CMAKE_MAP_IMPORTED_CONFIG_${config} ${CMAKE_MAP_IMPORTED_CONFIG_${config}})
    endif()
    set(CMAKE_MAP_IMPORTED_CONFIG_${config} ${ARGN})
endmacro()

macro(osvr_unstash_map_config config)
    if(NOT DEFINED OSVR_IN_MAP_CONFIG_STASH_${config})
        # Nobody actually called the matching stash...
            return()
    endif()
    # Other half of re-entrancy protection
    list(REMOVE_AT OSVR_IN_MAP_CONFIG_STASH_${config} -1)
    list(LENGTH OSVR_IN_MAP_CONFIG_STASH_${config} OSVR_IN_MAP_CONFIG_STASH_LEN)
    if(OSVR_IN_MAP_CONFIG_STASH_LEN GREATER 0)
        # someone still in here
        return()
    endif()

    # Restoration of CMAKE_MAP_IMPORTED_CONFIG_${config}
    if(DEFINED OSVR_OLD_CMAKE_MAP_IMPORTED_CONFIG_${config})
        set(CMAKE_MAP_IMPORTED_CONFIG_${config} ${OSVR_OLD_CMAKE_MAP_IMPORTED_CONFIG_${config}})
        unset(OSVR_OLD_CMAKE_MAP_IMPORTED_CONFIG_${config})
    else()
        unset(CMAKE_MAP_IMPORTED_CONFIG_${config})
    endif()
endmacro()

macro(osvr_stash_common_map_config)
    if(MSVC)
        # Can't do this - different runtimes, incompatible ABI, etc.
        set(OSVR_DEBUG_FALLBACK)
        osvr_stash_map_config(DEBUG DEBUG)
    else()
        set(OSVR_DEBUG_FALLBACK DEBUG)
        osvr_stash_map_config(DEBUG DEBUG RELWITHDEBINFO RELEASE MINSIZEREL NONE)
    endif()
    osvr_stash_map_config(RELEASE RELEASE RELWITHDEBINFO MINSIZEREL NONE ${OSVR_DEBUG_FALLBACK})
    osvr_stash_map_config(RELWITHDEBINFO RELWITHDEBINFO RELEASE MINSIZEREL NONE ${OSVR_DEBUG_FALLBACK})
    osvr_stash_map_config(MINSIZEREL MINSIZEREL RELEASE RELWITHDEBINFO NONE ${OSVR_DEBUG_FALLBACK})
    osvr_stash_map_config(NONE NONE RELEASE RELWITHDEBINFO MINSIZEREL ${OSVR_DEBUG_FALLBACK})
endmacro()

macro(osvr_unstash_common_map_config)
    osvr_unstash_map_config(DEBUG)
    osvr_unstash_map_config(RELEASE)
    osvr_unstash_map_config(RELWITHDEBINFO)
    osvr_unstash_map_config(MINSIZEREL)
    osvr_unstash_map_config(NONE)
endmacro()
