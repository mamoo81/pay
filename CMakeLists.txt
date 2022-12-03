# This file is part of the Flowee project
# Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
project(flowee_pay VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# calling find_package Qt two times seems to be needed to get the Qt version :shrug:
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Core Quick Svg REQUIRED
    OPTIONAL_COMPONENTS Multimedia)
find_package(flowee REQUIRED flowee_p2p)
find_package(OpenSSL REQUIRED)
if (ANDROID)
    option(quick_deploy "Do not include the blockchain in the APK, for a smaller (developer ONLY) deployment" OFF)
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
                "/opt/android-zxing/lib/libZXing.a;/opt/android-zxing/lib/libzueci-static.a")
        set (ZXing_FOUND TRUE)
    else ()
        find_package(ZXing REQUIRED)
    endif()
else ()
    find_package(Qt6 COMPONENTS DBus LinguistTools)
    find_package(ZXing)
endif()
find_package(Boost 1.67.0 REQUIRED filesystem chrono thread)
find_package(QREncode REQUIRED)

option(local_qml "Allow local QML loading" OFF)
option(networkLog "Include network-logging client" OFF)

add_compile_definitions(TARGET_OS_${CMAKE_SYSTEM_NAME})
# The cmake system name will hold values like Android, Linux or others.

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

set (PAY_SOURCES
    AccountInfo.cpp
    AddressInfo.cpp
    BitcoinValue.cpp
    FloweePay.cpp
    MenuModel.cpp
    NetDataProvider.cpp
    NetPeer.cpp
    NewWalletConfig.cpp
    NotificationManager.cpp
    Payment.cpp
    PaymentRequest.cpp
    PaymentDetailOutput.cpp
    PaymentDetailInputs.cpp
    PortfolioDataProvider.cpp
    PriceDataProvider.cpp
    PriceHistoryDataProvider.cpp
    QRCreator.cpp
    TransactionInfo.cpp
    Wallet.cpp
    WalletCoinsModel.cpp
    WalletHistoryModel.cpp
    WalletSecretsModel.cpp
    Wallet_encryption.cpp
    Wallet_support.cpp
    Wallet_spending.cpp
    WalletEnums.cpp
)

if (NetworkLogClient)
    list(APPEND PAY_SOURCES NetworkLogClient.cpp)
endif()

if (ANDROID)
    list(APPEND PAY_SOURCES main_utils_android.cpp)
else ()
    list(APPEND PAY_SOURCES main_utils.cpp)
endif ()

add_library(pay_lib STATIC ${PAY_SOURCES})

target_link_libraries(pay_lib
    flowee_apputils
    flowee_utils
    flowee_p2p
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    Qt6::Core Qt6::Quick ${Qt6DBus_LIBRARIES} ${PayLib_PRIVATE_LIBS} ${QREncode_LIBRARIES})

###### Translations

if(NOT ANDROID)
    # Skip for Android: Linguist is too big a dependency
    # We check lower if they have magically appeared: just use the native-build
    # and copy the resulting qm files to your android build.
    set (TS_FILES
        translations/floweepay-desktop_en.ts
        translations/floweepay-desktop_nl.ts
        translations/floweepay-desktop_pl.ts

        translations/floweepay-common_en.ts
        translations/floweepay-common_nl.ts
        translations/floweepay-common_pl.ts
        translations/floweepay-mobile_nl.ts
        translations/floweepay-mobile_pl.ts
    )
    qt6_add_translation(qmFiles ${TS_FILES})

    add_custom_target(i18n
        COMMAND lupdate guis/desktop.qrc -ts ${TS_FILES_DESKTOP} translations/floweepay-desktop.ts
        COMMAND lupdate ${PAY_SOURCES} guis/widgets.qrc
                    -ts ${TS_FILES_GENERIC} translations/floweepay-common.ts
        COMMAND lupdate guis/mobile.qrc -ts ${TS_FILES_MOBILE} translations/floweepay-mobile.ts
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT
            "Updating internationalization (i18n) translation files"
    )
endif()


###### Pay executable

if (NOT ANDROID)
    set (NOT_ANDROID TRUE)
endif()
option (BUILD_DESKTOP_PAY
    "Build the desktop (dev) client of Pay"
    ${NOT_ANDROID})

if (BUILD_DESKTOP_PAY)
    # The qm files are generated in the build tree, but the qrc file is inside the
    # source directory and the path to resources are relative to the location of
    # the qrc file itself. We copy the qrc file in the build
    # directory such that it can find the qm translations files. The qrc file is
    # copied if it doesn't exist in the destination or if it is modified.
    file(COPY translations/desktop-i18n.qrc DESTINATION ${CMAKE_BINARY_DIR})
    if (local_qml)
        set (QML_PATH ${CMAKE_SOURCE_DIR}/guis/)
    endif()
    configure_file(qml_path_helper.cpp.in guis/desktop/qml_path_helper.cpp)
    set (SOURCES_PAY
        main.cpp
        guis/desktop/qml_path_helper.cpp
    )
    qt6_add_resources(SOURCES_PAY
        guis/desktop.qrc
        guis/widgets.qrc
        ${CMAKE_BINARY_DIR}/desktop-i18n.qrc
    )
    add_executable(pay ${SOURCES_PAY})
    set_target_properties(pay PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} DESKTOP")
    target_link_libraries(pay pay_lib Qt6::Svg)
    install(TARGETS pay DESTINATION bin)
endif()

###### Pay-mobile executable
option (BUILD_MOBILE_PAY "Build the mobile client of Pay" ON)

if (${Qt6Multimedia_NOTFOUND} OR ${ZXing_NOTFOUND})
    set (BUILD_MOBILE_PAY OFF)
endif ()

if (ANDROID AND BUILD_MOBILE_PAY)
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

    configure_file(qml_path_helper.cpp.in guis/mobile/qml_path_helper.cpp)
    set (SOURCES_PAY_MOBILE
        main.cpp
        CameraController.cpp
        QRScanner.cpp
        guis/mobile/qml_path_helper.cpp
    )
    qt6_add_resources(SOURCES_PAY_MOBILE
        guis/mobile.qrc
        guis/widgets.qrc
    )
    if (EXISTS "${CMAKE_BINARY_DIR}/floweepay-mobile_nl.qm")
        message ("pay_mobile: Found pre-compiled translations")
        file(COPY translations/mobile-i18n.qrc DESTINATION ${CMAKE_BINARY_DIR})
        qt6_add_resources(SOURCES_PAY_MOBILE ${CMAKE_BINARY_DIR}/mobile-i18n.qrc)
    endif()

    qt6_add_executable(pay_mobile ${SOURCES_PAY_MOBILE})

    # For 6.2 - 6.4 the Qt Core permission APIs are only available as Android private apis
    if (${Qt6Multimedia_FOUND} AND ${QT_VERSION_MINOR} GREATER_EQUAL 2 AND ${QT_VERSION_MINOR} LESS_EQUAL 4)
        set (PAY_MOBILE_LIBS Qt6::CorePrivate)
        message(STATUS "including private QtCore APIs for Android support")
    endif ()
    target_link_libraries(pay_mobile pay_lib ZXing::ZXing Qt6::Svg Qt6::Multimedia ${PAY_MOBILE_LIBS})
    set_target_properties(pay_mobile PROPERTIES
       QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
       COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} MOBILE"
   )
    qt6_android_generate_deployment_settings(pay_mobile)
    qt6_android_add_apk_target(pay_mobile)
endif ()

if(NOT ANDROID AND BUILD_MOBILE_PAY)
    if(local_qml)
        set (QML_PATH ${CMAKE_SOURCE_DIR}/guis)
    endif()
    configure_file(qml_path_helper.cpp.in guis/mobile/qml_path_helper.cpp)
    file(COPY translations/mobile-i18n.qrc DESTINATION ${CMAKE_BINARY_DIR})

    set (SOURCES_PAY_MOBILE
        main.cpp
        CameraController.cpp
        QRScanner.cpp
        guis/mobile/qml_path_helper.cpp
    )
    qt6_add_resources(SOURCES_PAY_MOBILE
        guis/mobile.qrc
        guis/widgets.qrc
        ${CMAKE_BINARY_DIR}/mobile-i18n.qrc
    )
    add_executable(pay_mobile ${SOURCES_PAY_MOBILE})
    set_target_properties(pay_mobile PROPERTIES
       COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} MOBILE"
    )
    target_link_libraries(pay_mobile pay_lib ZXing::ZXing Qt6::Svg Qt6::Multimedia)
    install(TARGETS pay_mobile DESTINATION bin)
endif()


###### blockheaders-meta-extractor executable

option (BUILD_PAY_TOOLS
    "Build the tools Pay"
    ${NOT_ANDROID})

if (BUILD_PAY_TOOLS)
    add_executable(blockheaders-meta-extractor
        MetaExtractor.cpp
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

if (NOT ANDROID)
    add_subdirectory(testing)
endif()

# Report ------
message("")
message("Configuration results:")
message("----------------------")
message(STATUS "Target OS: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Qt version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
if (${local_qml})
    message("   Using QML from source-dir. DO NOT DISTRIBUTE BINARIES!")
endif ()
if (NetworkLogClient)
    message ("-> Including network-logging capability")
    add_compile_definitions(NETWORK_LOGGER)
endif()
if (${BUILD_DESKTOP_PAY})
    message ("-> Building Desktop-Pay...")
    if (${Qt6DBus_FOUND})
        message("   Found optional lib: QtDBus")
    else ()
        message("   Missing QtDBus, skipping support for desktop notifications")
    endif ()
endif()
if (${Qt6Multimedia_NOTFOUND})
    message("ww Missing QtMultimedia libs, not building Pay for mobile ")
endif ()
if (${ZXing_NOTFOUND})
    message("ww Missing ZXing lib, not building Pay for mobile ")
endif ()
if (${BUILD_MOBILE_PAY})
    message ("-> Building Pay for mobile")
    if (ANDROID AND DEFINED MOBILE_PAY_I18N_QRC)
        message ("   Found translation files, including in package")
    endif ()
endif()
if (${BUILD_PAY_TOOLS})
    message ("-> Building Pay tools")
endif()

message("")
message("")
