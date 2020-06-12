# This file is part of the Flowee project
# Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

find_package(Qt5 COMPONENTS Core Quick REQUIRED)

set (PAY_SOURCES
    FloweePay.cpp
    main.cpp
    NetDataProvider.cpp
    NetPeer.cpp
    PortfolioDataProvider.cpp
    Wallet.cpp
    WalletHistoryModel.cpp
    BitcoinValue.cpp
)

if(ANDROID)
    add_library(PAY SHARED
        ${PAY_SOURCES}
    )
else()
    add_executable(pay
        ${PAY_SOURCES}
        desktop/qml.qrc
    )
endif()

find_package(Boost 1.56.0
    REQUIRED
    filesystem chrono thread
    # iostreams 
)
find_package(OpenSSL REQUIRED)
message ("Using OpenSSL ${OPENSSL_VERSION}")
find_package(Flowee REQUIRED flowee_p2p)

target_compile_definitions(pay
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(pay
    flowee_utils
    flowee_p2p
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    Qt5::Core Qt5::Quick)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/pay DESTINATION bin)
