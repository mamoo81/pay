# This file is part of the Flowee project
# Copyright (C) 2020 Tom Zander <tom@flowee.org>
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

cmake_minimum_required(VERSION 3.5)
project(flowee_pay)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(Qt5 COMPONENTS Core Quick LinguistTools REQUIRED)
find_package(QREncode REQUIRED)

# starting with Qt5.15 we have a lot of deprecation warnings,
# likely to make porting to Qt6 easier.
# as long as we require linking to older Qt versions those warnings
# are clutter. Remove this define when we start porting to Qt6.
add_definitions(-DQT_NO_DEPRECATED_WARNINGS)


set (PAY_SOURCES
    AccountInfo.cpp
    BitcoinValue.cpp
    FloweePay.cpp
    main.cpp
    NetDataProvider.cpp
    NetPeer.cpp
    NewWalletConfig.cpp
    NotificationManager.cpp
    Payment.cpp
    PaymentRequest.cpp
    PortfolioDataProvider.cpp
    PriceDataProvider.cpp
    QRCreator.cpp
    TransactionInfo.cpp
    Wallet.cpp
    WalletCoinsModel.cpp
    WalletHistoryModel.cpp
    WalletSecretsModel.cpp
    Wallet_support.cpp
    Wallet_spending.cpp
)

if (EXISTS ${CMAKE_SOURCE_DIR}/data/blockheaders)
    install(FILES ${CMAKE_SOURCE_DIR}/data/blockheaders DESTINATION share/floweepay)
endif()

if(ANDROID)
    add_library(PAY SHARED
        ${PAY_SOURCES}
    )
else()
    set (TS_FILES
        desktop/i18n/floweepay_nl.ts
        desktop/i18n/floweepay_pl.ts
    )
    # set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "l10n")
    qt5_add_translation(qmFiles ${TS_FILES})

    # notice that for this custom target we require CMake 3.13 minimum. But since
    # only the release manager will need to call this, we leave the project cmake
    # minimum to be lower.
    add_custom_target(i18n
        COMMAND lupdate desktop/qml.qrc ${PAY_SOURCES} -ts ${TS_FILES} desktop/i18n/floweepay_en.ts
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT
            "Updating internationalization (i18n) translation files"
    )

    # The qm files are generated in the build tree, but the qrc file is inside the
    # source directory and the path to resources are relative to the location of
    # the qrc file itself. We use configure_file() to copy the qrc file in the build
    # directory such that it can find the qm translations files. The qrc file is
    # copied if it doesn't exist in the destination or if it is modified.
    configure_file(desktop/i18n.qrc ${CMAKE_BINARY_DIR} COPYONLY)
    add_executable(pay
        ${PAY_SOURCES}
        ${QM_FILES}
        desktop/qml.qrc
        i18n.qrc
    )
    install(PROGRAMS desktop/org.flowee.pay.desktop DESTINATION share/applications)
    set (ICONIN desktop/icons/hicolor/)
    set (ICONOUT share/icons/hicolor/)
    install(FILES ${ICONIN}16x16/apps/org.flowee.pay.png DESTINATION ${ICONOUT}16x16/apps)
    install(FILES ${ICONIN}22x22/apps/org.flowee.pay.png DESTINATION ${ICONOUT}22x22/apps)
    install(FILES ${ICONIN}24x24/apps/org.flowee.pay.png DESTINATION ${ICONOUT}24x24/apps)
    install(FILES ${ICONIN}32x32/apps/org.flowee.pay.png DESTINATION ${ICONOUT}32x32/apps)
    install(FILES ${ICONIN}48x48/apps/org.flowee.pay.png DESTINATION ${ICONOUT}48x48/apps)
    install(FILES ${ICONIN}256x256/apps/org.flowee.pay.png DESTINATION ${ICONOUT}256x256/apps)
endif()

find_package(Boost 1.67.0
    REQUIRED
    filesystem chrono thread
)
find_package(OpenSSL REQUIRED)
message ("Using OpenSSL ${OPENSSL_VERSION}")
find_package(Flowee REQUIRED flowee_p2p)

target_compile_definitions(pay
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(pay
    flowee_apputils
    flowee_utils
    flowee_p2p
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    Qt5::Core Qt5::Quick ${QREncode_LIBRARIES})

add_subdirectory(testing)

install(FILES ${CMAKE_SOURCE_DIR}/data/bip39-english DESTINATION share/floweepay)
install(TARGETS pay DESTINATION bin)
