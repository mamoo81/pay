# Copyright (c) 2019-2020 The Bitcoin developers

#.rst
# FindQREncode
# -------------
#
# Find the QREncode library. The following
# components are available::
#   qrencode
#
# This will define the following variables::
#
#   QREncode_FOUND - system has QREncode lib
#   QREncode_INCLUDE_DIRS - the QREncode include directories
#   QREncode_LIBRARIES - Libraries needed to use QREncode
#
# And the following imported target::
#
#   QREncode::qrencode

include(BrewHelper)
find_brew_prefix(BREW_HINT qrencode)

find_package(PkgConfig)
pkg_check_modules(PC_QREncode QUIET libqrencode)

if (ANDROID)
    if (EXISTS "/opt/android-qrencode/include/qrencode.h")
        set (QREncode_INCLUDE_DIR "/opt/android-qrencode/include")
        set (QREncode_FOUND TRUE)
        set (QREncode_LIBRARIES "/opt/android-qrencode/lib/libqrencode.a")
    endif()
else()
    find_path(QREncode_INCLUDE_DIR
        NAMES qrencode.h
        HINTS ${BREW_HINT}
        PATHS ${PC_QREncode_INCLUDE_DIRS}
    )
endif()

set(QREncode_INCLUDE_DIRS "${QREncode_INCLUDE_DIR}")
mark_as_advanced(QREncode_INCLUDE_DIR)

include(ExternalLibraryHelper)
find_component(QREncode qrencode
    NAMES qrencode
    HINTS ${BREW_HINT}
    PATHS ${PC_QREncode_LIBRARY_DIR}
    INCLUDE_DIRS ${QREncode_INCLUDE_DIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QREncode
	REQUIRED_VARS
		QREncode_INCLUDE_DIR
	HANDLE_COMPONENTS
)
include_directories(${QREncode_INCLUDE_DIR})
