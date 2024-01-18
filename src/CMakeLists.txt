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

project(flowee_lib)

set (PAY_SOURCES
    AccountInfo.cpp
    AddressInfo.cpp
    BitcoinValue.cpp
    FloweePay.cpp
    MenuModel.cpp
    NetDataProvider.cpp
    NewWalletConfig.cpp
    NotificationManager.cpp
    Payment.cpp
    PaymentBackend.cpp
    PaymentRequest.cpp
    PaymentDetailOutput.cpp
    PaymentDetailInputs.cpp

    PaymentProtocol.cpp
    PortfolioDataProvider.cpp
    PriceDataProvider.cpp
    PriceHistoryDataProvider.cpp
    QRCreator.cpp
    QMLClipboardHelper.cpp
    TransactionInfo.cpp
    TxInfoObject.cpp
    Wallet.cpp
    WalletCoinsModel.cpp
    AccountConfig.cpp
    WalletHistoryModel.cpp
    WalletKeyView.cpp
    WalletSecretsModel.cpp
    Wallet_encryption.cpp
    Wallet_support.cpp
    Wallet_spending.cpp
    WalletEnums.cpp

    # Modules support
    ModuleInfo.cpp
    ModuleManager.cpp
    ModuleSection.cpp
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
    Qt6::Core Qt6::Quick ${Qt6DBus_LIBRARIES} ${PayLib_PRIVATE_LIBS} ZXing::ZXing )
