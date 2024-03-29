# This file is part of the Flowee project
# Copyright (C) 2020-2024 Tom Zander <tom@flowee.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.19)
project(flowee_pay VERSION 1.0 LANGUAGES CXX)

if (NOT ANDROID)
    set (NOT_ANDROID TRUE)
endif()
option(skip_example "Make a release build for Android, with the release package name" OFF)
option(quick_deploy "Do not include the blockchain in the APK, for a smaller (developer ONLY) deployment" OFF)
option(build_desktop_pay "Build the desktop (dev) client of Pay" ${NOT_ANDROID})
option(build_mobile_pay "Build the mobile client of Pay" ON)
option(build_pay_tools "Build the tools Pay" ${NOT_ANDROID})

# options from src/CMakeLists.txt
option(local_qml "Allow local QML loading" OFF)
option(NetworkLogClient "Include the network based logging in the executables" OFF)



set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

# calling find_package Qt two times seems to be needed to get the Qt version :shrug:
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Core Quick Svg REQUIRED
    OPTIONAL_COMPONENTS Multimedia)
find_package(flowee REQUIRED flowee_p2p)
find_package(OpenSSL REQUIRED)

if (ANDROID)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
        ${CMAKE_SOURCE_DIR}/android/cmake)

    # Config ZXing here.
    if (EXISTS "/opt/android-zxing/include/ZXing/DecodeHints.h")
        add_library(ZXing::ZXing STATIC IMPORTED)
        set_property(TARGET ZXing::ZXing PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "/opt/android-zxing/include")
        set_target_properties(ZXing::ZXing PROPERTIES IMPORTED_LOCATION
                "/opt/android-zxing/lib/libZXing.a")
        set_property(TARGET ZXing::ZXing PROPERTY IMPORTED_LINK_INTERFACE_LIBRARIES
                "/opt/android-zxing/lib/libZXing.a")
        set (ZXing_FOUND TRUE)
    else ()
        find_package(ZXing REQUIRED)
    endif()
else ()
    find_package(Qt6 COMPONENTS DBus LinguistTools)
    find_package(ZXing REQUIRED)
endif()
find_package(Boost 1.67.0 REQUIRED filesystem chrono thread)
include_directories(${Boost_INCLUDE_DIRS})

function(download_file url path)
    if (NOT EXISTS "${path}")
        get_filename_component(file "${path}" NAME)
        message(STATUS "Downloading file ${file} from ${url}")

        file(DOWNLOAD "${url}" "${path}" INACTIVITY_TIMEOUT 10 STATUS download_result)
        list(GET download_result 0 status_code)
        list(GET download_result 1 error_message)

        if (NOT status_code EQUAL 0)
            file(REMOVE "${path}")
            message(FATAL_ERROR "Failed to download ${url}: ${error_message}")
        endif()
    endif()
endfunction()

# The cmake system name will hold values like Android, Linux or others.
add_compile_definitions(TARGET_OS_${CMAKE_SYSTEM_NAME})

###### Translations

if(NOT ANDROID)
    # Skip for Android: Linguist is too big a dependency
    # We check lower if they have magically appeared: just use the native-build
    # and copy the resulting qm files to your android build.
    set (TS_FILES
        translations/floweepay-desktop_en.ts
        translations/floweepay-desktop_nl.ts
        translations/floweepay-desktop_pl.ts
        translations/floweepay-desktop_de.ts
        translations/floweepay-desktop_es.ts
        translations/floweepay-desktop_pt.ts
        translations/floweepay-desktop_ha.ts

        translations/floweepay-common_en.ts
        translations/floweepay-common_nl.ts
        translations/floweepay-common_pl.ts
        translations/floweepay-common_de.ts
        translations/floweepay-common_es.ts
        translations/floweepay-common_pt.ts
        translations/floweepay-common_ha.ts

        translations/floweepay-mobile_en.ts
        translations/floweepay-mobile_nl.ts
        translations/floweepay-mobile_pl.ts
        translations/floweepay-mobile_de.ts
        translations/floweepay-mobile_es.ts
        translations/floweepay-mobile_pt.ts
        translations/floweepay-mobile_ha.ts

        translations/module-build-transaction_en.ts
        translations/module-build-transaction_nl.ts
        translations/module-build-transaction_pl.ts
        translations/module-build-transaction_de.ts
        translations/module-build-transaction_es.ts
        translations/module-build-transaction_pt.ts
        translations/module-build-transaction_ha.ts

        translations/module-peers-view_en.ts
        translations/module-peers-view_nl.ts
        translations/module-peers-view_pl.ts
        translations/module-peers-view_de.ts
        translations/module-peers-view_es.ts
        translations/module-peers-view_pt.ts
        translations/module-peers-view_ha.ts
    )
    qt6_add_translation(qmFiles ${TS_FILES})

    add_custom_target(i18n
        COMMAND lupdate guis/desktop.qrc -ts translations/floweepay-desktop.ts
        COMMAND lupdate src guis/widgets.qrc
                    -ts translations/floweepay-common.ts
        COMMAND lupdate guis/mobile.qrc -ts translations/floweepay-mobile.ts
        COMMAND lupdate modules/build-transaction/build-transactions-data.qrc
                        modules/build-transaction/BuildTransactionModuleInfo.cpp
                        -ts translations/module-build-transaction.ts
        COMMAND lupdate modules/peers-view/peers-page-data.qrc
                        modules/peers-view/PeersViewModuleInfo.cpp
                        -ts translations/module-peers-view.ts
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT
            "Updating internationalization (i18n) translation files"
    )
endif()

add_compile_definitions(LOG_DEFAULT_SECTION=10000)
add_compile_definitions(LOG_WALLET=10001)
if (NetworkLogClient)
    add_compile_definitions(NETWORK_LOGGER)
elseif (ANDROID)
    # include much less logging in the codebase, no point since we can't access it anyway
    add_compile_definitions(BCH_NO_DEBUG_OUTPUT BCH_NO_INFO_OUTPUT BCH_NO_WARNING_OUTPUT)
endif()

###### Pay executable
include_directories(${CMAKE_SOURCE_DIR}/src)

# local QML means we hardcode the full path to the 'guis' dir for this build.
# and instead of pointing to the QRC, we point here. Avoding re-compile / linking
# after updating the QML files.
if (local_qml)
    set (QML_PATH ${CMAKE_SOURCE_DIR}/guis)
endif()
configure_file(src/qml_path_helper.cpp.in src/qml_path_helper.cpp)

if (build_desktop_pay)
    # The qm files are generated in the build tree, but the qrc file is inside the
    # source directory and the path to resources are relative to the location of
    # the qrc file itself. We copy the qrc file in the build
    # directory such that it can find the qm translations files. The qrc file is
    # copied if it doesn't exist in the destination or if it is modified.
    file(COPY translations/desktop-i18n.qrc DESTINATION ${CMAKE_BINARY_DIR})
    set (SOURCES_PAY
        src/main.cpp
        ${CMAKE_BINARY_DIR}/src/qml_path_helper.cpp
        src/ModuleManager_empty.cpp # because we don't have modules in the desktop one
    )
    qt6_add_resources(SOURCES_PAY
        guis/desktop.qrc
        guis/widgets.qrc
        ${CMAKE_BINARY_DIR}/desktop-i18n.qrc
    )
    add_executable(pay ${SOURCES_PAY})
    set_target_properties(pay PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} DESKTOP")
    target_link_libraries(pay PRIVATE pay_lib Qt6::Svg)
    install(TARGETS pay DESTINATION bin)
endif()

###### Pay-mobile executable

if (NOT "${Qt6Multimedia_FOUND}")
    set (build_mobile_pay OFF)
endif ()

set (PAY_MOBILE_LIBS "")
set (PAY_MOBILE_RESOURCES "")
if (build_mobile_pay)  # support modules, which get enabled currently for mobile only
    add_subdirectory(modules)

    # find all modules present in the source-tree and make sure we link them into the
    # flowee-pay-mobile executable.
    # Notice that the 'modules' subdir has similar code to actually compile these libs.
    file(GLOB _module_sub_directories ${CMAKE_CURRENT_SOURCE_DIR}/modules/*)
    foreach (child ${_module_sub_directories})
        if (IS_DIRECTORY ${child} AND NOT IS_SYMLINK ${child})
            file(GLOB subModule ${child}/*ModuleInfo.h)
            file(RELATIVE_PATH moduleName ${CMAKE_CURRENT_SOURCE_DIR}/modules ${child})

            if (NOT (${skip_example} AND ${moduleName} STREQUAL "example"))
                list(APPEND PAY_MOBILE_LIBS "${moduleName}_module_lib")
                file(GLOB resources ${child}/*qrc)
                list(APPEND PAY_MOBILE_RESOURCES "${resources}")
            endif()
        endif()
    endforeach()

    include_directories(${CMAKE_SOURCE_DIR}/modules)
endif()


if (ANDROID AND build_mobile_pay)
    # blockheaders to be included in the APK
    set (ASSETS_DIR ${CMAKE_BINARY_DIR}/android-build/assets/)
    if (NOT quick_deploy)
        download_file(https://flowee.org/products/pay/blockheaders
            ${ASSETS_DIR}/blockheaders)
    endif()
    file(COPY ${CMAKE_SOURCE_DIR}/data/bip39-english
              ${CMAKE_SOURCE_DIR}/data/bip39-chinese_simplified
              ${CMAKE_SOURCE_DIR}/data/bip39-chinese_traditional
              ${CMAKE_SOURCE_DIR}/data/bip39-czech
              ${CMAKE_SOURCE_DIR}/data/bip39-french
              ${CMAKE_SOURCE_DIR}/data/bip39-italian
              ${CMAKE_SOURCE_DIR}/data/bip39-japanese
              ${CMAKE_SOURCE_DIR}/data/bip39-korean
              ${CMAKE_SOURCE_DIR}/data/bip39-portuguese
              ${CMAKE_SOURCE_DIR}/data/bip39-spanish
        DESTINATION ${ASSETS_DIR})

    set (SOURCES_PAY_MOBILE
        src/main.cpp
        src/CameraController.cpp
        src/QRScanner.cpp
        ${CMAKE_BINARY_DIR}/src/qml_path_helper.cpp
        ${CMAKE_BINARY_DIR}/modules/modules-load.cpp
    )
    qt6_add_resources(SOURCES_PAY_MOBILE
        guis/mobile.qrc
        guis/widgets.qrc
        ${PAY_MOBILE_RESOURCES}
    )
    if (EXISTS "${CMAKE_BINARY_DIR}/floweepay-mobile_nl.qm")
        message ("pay_mobile: Found pre-compiled translations")
        file(COPY translations/mobile-i18n.qrc DESTINATION ${CMAKE_BINARY_DIR})
        qt6_add_resources(SOURCES_PAY_MOBILE ${CMAKE_BINARY_DIR}/mobile-i18n.qrc)
    endif()

    qt6_add_executable(pay_mobile ${SOURCES_PAY_MOBILE})
    qt_import_plugins(pay_mobile
        EXCLUDE_BY_TYPE
            imageformats
            qmltooling
            networkinformation
        INCLUDE
            Qt::QSvgPlugin
    )

    target_link_libraries(pay_mobile PRIVATE pay_lib Qt6::Svg Qt6::Multimedia ${PAY_MOBILE_LIBS})
    set_target_properties(pay_mobile PROPERTIES
       QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android
       COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} MOBILE"
    )
endif ()

if(NOT ANDROID AND build_mobile_pay)
    file(COPY translations/mobile-i18n.qrc DESTINATION ${CMAKE_BINARY_DIR})

    set (SOURCES_PAY_MOBILE
        src/main.cpp
        src/CameraController.cpp
        src/QRScanner.cpp
        ${CMAKE_BINARY_DIR}/modules/modules-load.cpp
        ${CMAKE_BINARY_DIR}/src/qml_path_helper.cpp
    )
    qt6_add_resources(SOURCES_PAY_MOBILE
        guis/mobile.qrc
        guis/widgets.qrc
        ${CMAKE_BINARY_DIR}/mobile-i18n.qrc
        ${PAY_MOBILE_RESOURCES}
    )
    add_executable(pay_mobile ${SOURCES_PAY_MOBILE})
    set_target_properties(pay_mobile PROPERTIES
       COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} MOBILE"
    )

    target_link_libraries(pay_mobile PRIVATE pay_lib Qt6::Svg Qt6::Multimedia ${PAY_MOBILE_LIBS})
    install(TARGETS pay_mobile DESTINATION bin)
endif()


###### blockheaders-meta-extractor executable

if (build_pay_tools)
    add_executable(blockheaders-meta-extractor
        src/MetaExtractor.cpp
    )
    target_link_libraries(blockheaders-meta-extractor
        flowee_p2p
        flowee_utils
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
    )
    install(TARGETS blockheaders-meta-extractor DESTINATION bin)
endif()

install(PROGRAMS guis/desktop/org.flowee.pay.desktop DESTINATION share/applications)
set (ICONIN guis/desktop/icons/hicolor/)
set (ICONOUT share/icons/hicolor/)
install(FILES ${ICONIN}16x16/apps/org.flowee.pay.png DESTINATION ${ICONOUT}16x16/apps)
install(FILES ${ICONIN}22x22/apps/org.flowee.pay.png DESTINATION ${ICONOUT}22x22/apps)
install(FILES ${ICONIN}24x24/apps/org.flowee.pay.png DESTINATION ${ICONOUT}24x24/apps)
install(FILES ${ICONIN}32x32/apps/org.flowee.pay.png DESTINATION ${ICONOUT}32x32/apps)
install(FILES ${ICONIN}48x48/apps/org.flowee.pay.png DESTINATION ${ICONOUT}48x48/apps)
install(FILES ${ICONIN}256x256/apps/org.flowee.pay.png DESTINATION ${ICONOUT}256x256/apps)
install(FILES ${CMAKE_SOURCE_DIR}/data/bip39-english DESTINATION share/floweepay)
if (EXISTS ${CMAKE_SOURCE_DIR}/data/blockheaders)
    install(FILES ${CMAKE_SOURCE_DIR}/data/blockheaders DESTINATION share/floweepay)
endif()

add_subdirectory(src)
if (NOT ANDROID)
    add_subdirectory(testing)
endif()

# Report ------
message("")
message("Configuration results:")
message("----------------------")
message("Target OS: ${CMAKE_SYSTEM_NAME}")
message("Qt version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
if (${local_qml})
    message("   Using QML from source-dir. DO NOT DISTRIBUTE BINARIES!")
endif ()
if (build_desktop_pay)
    message ("-> Building Desktop-Pay...")
    if (${Qt6DBus_FOUND})
        message("   Found optional lib: QtDBus")
    else ()
        message("   Missing QtDBus, skipping support for desktop notifications")
    endif ()
endif()
if (NOT ${Qt6Multimedia_FOUND})
    message("ww Missing QtMultimedia libs, not building Pay for mobile ")
endif ()
if (${build_mobile_pay})
    message ("-> Building Pay for mobile")
    if (ANDROID AND ${skip_example})
        message ("   With release package-ID!")
    endif()
    if (ANDROID AND DEFINED MOBILE_PAY_I18N_QRC)
        message ("   Found translation files, including in package")
    endif ()
endif()
if (NetworkLogClient)
    message ("-> Including network-logging capability")
endif()
if (build_pay_tools)
    message ("-> Building Pay tools")
endif()

if (${build_mobile_pay})
    file(GLOB sub_directories ${CMAKE_SOURCE_DIR}/modules/*)
    foreach (child ${sub_directories})
        if (IS_DIRECTORY ${child})
            file(GLOB subModule ${child}/*ModuleInfo.h)
            if (EXISTS ${subModule})
                file (RELATIVE_PATH module ${CMAKE_CURRENT_SOURCE_DIR} ${child})
                if (NOT (${skip_example} AND ${module} STREQUAL "modules/example"))
                    message("-> Including module '${module}'")
                    get_filename_component(className ${subModule} NAME_WE)
                    message("   ${className}")
                endif()
            endif()
        endif()
    endforeach()
endif()

message("")
message("")
