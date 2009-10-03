# Find Detours includes and library
#
# This module defines
#  DETOURS_INCLUDE_DIR
#  DETOURS_LIBRARIES, the library to link against to use detours.
#  DETOURS_LIBARY_DIR
#  DETOURS_DLL, the location of the library
#  DETOURS_FOUND, If false, do not try to use detours


IF (WIN32) #Windows
    MESSAGE(STATUS "Looking for Detours")
    FIND_PATH(    DETOURS_INCLUDE_DIR detours.h    "C:/Program Files/Microsoft Research/Detours Express 2.1/include")
    FIND_PATH( DETOURS_LIBRARY_DIR detours.lib "C:/Program Files/Microsoft Research/Detours Express 2.1/lib")
    FIND_FILE(    DETOURS_DLL         detoured.dll "C:/Program Files/Microsoft Research/Detours Express 2.1/bin")

ENDIF (WIN32)


IF (DETOURS_INCLUDE_DIR AND DETOURS_LIBRARY_DIR AND DETOURS_DLL)
    SET(DETOURS_FOUND TRUE)
    SET(DETOURS_LIBRARIES detoured.lib detours.lib)
ENDIF ()

IF (DETOURS_FOUND)

    MESSAGE(STATUS "  libraries : ${DETOURS_LIBRARIES}")
    MESSAGE(STATUS "  includes  : ${DETOURS_INCLUDE_DIR}")

ELSE ()
    IF (DETOURS_FIND_REQUIRED)
        message("${DETOURS_INCLUDE_DIR}")
        MESSAGE("${DETOURS_LIBRARIES}")
        MESSAGE(FATAL_ERROR "Could not find DETOURS")
    ENDIF ()
ENDIF ()