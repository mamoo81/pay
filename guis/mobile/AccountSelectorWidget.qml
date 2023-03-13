/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee

Rectangle {
    id: root
    x: -10
    width: parent.width + 20
    height: {
        if (portfolio.singleAccountSetup)
            return currentWalletValue.height + 10
        return currentWalletValue.height + currentWalletLabel.height + 25
    }
    color: palette.alternateBase

    Flowee.HamburgerMenu {
        x: 10
        anchors.verticalCenter: currentWalletLabel.verticalCenter
        visible: portfolio.accounts.length > 1
    }

    Flowee.Label {
        id: currentWalletLabel
        y: 10
        x: 20
        width: parent.width - 30
        text: payment.account.name
        visible: !portfolio.singleAccountSetup
    }

    Flowee.BitcoinAmountLabel {
        id: currentWalletValue
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        value: {
            var wallet = payment.account;
            return wallet.balanceConfirmed + wallet.balanceUnconfirmed;
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: (mouse) => {
                       if (mouse.x < parent.width / 2) {
                           if (portfolio.accounts.length > 1)
                           accountSelector.open();
                       } else {
                           while (priceMenu.count > 0) {
                               priceMenu.takeItem(0);
                           }
                           priceMenu.addAction(sendAllAction);
                           priceMenu.x = root.width / 2
                           priceMenu.y = -40
                           priceMenu.open();
                       }
                   }
    }
    QQC2.Menu {
        id: priceMenu
    }

    AccountSelectorPopup {
        id: accountSelector
        width: root.width
        y: -10
        onSelectedAccountChanged: payment.account = selectedAccount
        selectedAccount: payment.account
    }
}
