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

cmake_minimum_required(VERSION 3.13)
project(flowee_pay)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

find_package(Qt6 COMPONENTS Core Quick REQUIRED)
find_package(flowee REQUIRED flowee_p2p)
find_package(OpenSSL REQUIRED)
if (ANDROID)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
        ${CMAKE_SOURCE_DIR}/android/cmake)
else ()
    find_package(Qt6 COMPONENTS DBus LinguistTools)
endif()
find_package(Boost 1.67.0 REQUIRED filesystem chrono thread)
find_package(QREncode REQUIRED)

option(local_qml "Allow local QML loading" OFF)

set (PAY_SOURCES
    AccountInfo.cpp
    AddressInfo.cpp
    BitcoinValue.cpp
    FloweePay.cpp
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
)

add_library(pay_lib STATIC
    ${PAY_SOURCES}
)

target_link_libraries(pay_lib
    flowee_apputils
    flowee_utils
    flowee_p2p
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    Qt6::Core Qt6::Quick ${Qt6DBus_LIBRARIES} ${QREncode_LIBRARIES})

#### Translations

if(NOT ANDROID)
    # Linguist is too big a dependency, just use the native-build to get the qm files
    set (TS_FILES_DESKTOP
        translations/floweepay-desktop_en.ts
        translations/floweepay-desktop_nl.ts
        translations/floweepay-desktop_pl.ts
    )
    qt6_add_translation(qmFiles1 ${TS_FILES_DESKTOP})
    set (TS_FILES_COMMON
        translations/floweepay-common_en.ts
        translations/floweepay-common_nl.ts
        translations/floweepay-common_pl.ts
    )
    qt6_add_translation(qmFiles2 ${TS_FILES_COMMON})
    set (TS_FILES_MOBILE
        translations/floweepay-mobile_nl.ts
        translations/floweepay-mobile_pl.ts
    )
    qt6_add_translation(qmFiles3 ${TS_FILES_MOBILE})

    add_custom_target(i18n
        COMMAND lupdate desktop/qml.qrc -ts ${TS_FILES_DESKTOP} translations/floweepay-desktop.ts
        COMMAND lupdate ${PAY_SOURCES} -ts ${TS_FILES_GENERIC} translations/floweepay-common.ts
        COMMAND lupdate mobile/qml.qrc -ts ${TS_FILES_MOBILE} translations/floweepay-mobile.ts
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

if(BUILD_DESKTOP_PAY)
    # The qm files are generated in the build tree, but the qrc file is inside the
    # source directory and the path to resources are relative to the location of
    # the qrc file itself. We use configure_file() to copy the qrc file in the build
    # directory such that it can find the qm translations files. The qrc file is
    # copied if it doesn't exist in the destination or if it is modified.
    configure_file(translations/desktop-i18n.qrc ${CMAKE_BINARY_DIR} COPYONLY)
    if (local_qml)
        set (QML_PATH ${CMAKE_SOURCE_DIR}/desktop)
    endif()
    configure_file(qml_path_helper.cpp.in desktop/qml_path_helper.cpp)
    add_executable(pay
        main.cpp
        desktop/qml_path_helper.cpp
        desktop/qml.qrc
        desktop-i18n.qrc
    )
    target_link_libraries(pay pay_lib)
    install(TARGETS pay DESTINATION bin)
endif()


###### Pay-mobile executable
option (BUILD_MOBILE_PAY "Build the mobile client of Pay" TRUE)

if(BUILD_MOBILE_PAY)
    configure_file(translations/mobile-i18n.qrc ${CMAKE_BINARY_DIR} COPYONLY)
    if (local_qml)
        set (QML_PATH ${CMAKE_SOURCE_DIR}/mobile)
    endif()
    configure_file(qml_path_helper.cpp.in mobile/qml_path_helper.cpp)
    if(ANDROID)
        # the dependency of linguist is pretty steep for cross-compilation
        # so lets accept the lack of compiled translations here.
        # A build that wants them should copy them from a native build.

        if (EXISTS "${CMAKE_BINARY_DIR}/floweepay-mobile_nl.qml")
            set (MOBILE_PAY_I18N_QRC mobile-i18n.qrc)
        endif()
    endif()
    add_executable(pay_mobile
        main.cpp
        mobile/qml_path_helper.cpp
        mobile/qml.qrc
        ${MOBILE_PAY_I18N_QRC}
    )
    target_link_libraries(pay_mobile pay_lib)
    install(TARGETS pay_mobile DESTINATION bin)
endif()


###### blockheaders-meta-extractor executable

option (BUILD_PAY_TOOLS
    "Build the tools Pay"
    ${NOT_ANDROID})

if(BUILD_PAY_TOOLS)
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

install(PROGRAMS desktop/org.flowee.pay.desktop DESTINATION share/applications)
set (ICONIN desktop/icons/hicolor/)
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
if (${BUILD_DESKTOP_PAY})
    message ("-> Building Desktop-Pay...")
    if (${Qt6DBus_FOUND})
        message("   Found optional lib: QtDBus")
    else ()
        message("   Missing QtDBus, skipping support for desktop notifications")
    endif ()
endif()
if (${BUILD_MOBILE_PAY})
    message ("-> Building Pay for mobile")
endif()
if (${BUILD_PAY_TOOLS})
    message ("-> Building Pay tools")
endif()
if (${local_qml})
    message("   Using QML from source-dir. DO NOT DISTRIBUTE BINARIES!")
endif ()

message("")
message("")
