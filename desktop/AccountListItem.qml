/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "widgets" as Flowee

Item {
    id: root
    property QtObject account: null
    height: column.height + 26
    signal clicked;

    //background
    Rectangle {
        property bool selected: portfolio.current === account

        anchors.fill: parent
        anchors.margins: 5
        id: background
        property bool hover: false
        radius: 10
        color: selected && !Pay.useDarkSkin ? "white" : "#00000000" // transparant
        border.width: 3
        border.color: {
            if (portfolio.current === account)
                return mainWindow.floweeGreen
            if (hover)
                return Pay.useDarkSkin ? mainWindow.floweeSalmon : mainWindow.floweeBlue;
            return Pay.useDarkSkin ? "#EEE" : "#7b7f7f"
        }
    }

    Column {
        id: column
        spacing: 4
        x: 30
        y: 12
        width: parent.width - 40
        /*
        Label {
            text: "Savings Account"
            opacity: 0.6
        }*/
        Label {
            text: root.account.name
            font.bold: true
            width: parent.width - 20
            clip: true
        }
        WalletEncryptionStatus {
            id: walletEncryptionStatus
            width: parent.width
            account: root.account
        }
        Flow {
            width: parent.width
            spacing: 10
            visible: lastReceive.text !== "" && !root.account.isArchived
            Label {
                text: qsTr("Last Transaction") + ":"
            }
            Label {
                id: lastReceive
                text: Pay.formatDate(account.lastMinedTransaction)
            }
        }
        Label {
            visible: root.account.isArchived
            text: visible ? account.timeBehind : ""
        }
    }

    Rectangle {
        color: Pay.useDarkSkin ? mainWindow.floweeSalmon : mainWindow.floweeBlue
        width: 16
        height: 16
        radius: 8
        x: 14
        anchors.verticalCenter: parent.verticalCenter

        visible: {
            if (portfolio.current === root.account)
                return false;

            return root.account.hasFreshTransactions
        }
    }

    Flowee.ArrowPoint {
        id: point
        x: 5
        visible: portfolio.current === account;
        anchors.verticalCenter: parent.verticalCenter
        color: background.border.color
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            portfolio.current = account
            root.clicked()
        }
        onEntered: background.hover = true
        onExited: background.hover = false
    }

    ConfigItem {
        id: configItem
        anchors.right: parent.right
        anchors.rightMargin: 15
        y: 13

        property QtObject detailsAction: Action {
                text: qsTr("Details")
                onTriggered: {
                    portfolio.current = root.account;
                    accountOverlay.state = "accountDetails";
                }
            }
        property QtObject archiveAction: Action {
                text: root.account.isArchived ? qsTr("Unarchive") : qsTr("Archive Wallet")
                onTriggered: root.account.isArchived = !root.account.isArchived
            }
        property QtObject primaryAction: Action {
                enabled: !root.account.isDefaultWallet
                text: enabled ? qsTr("Make Primary") : qsTr("★ Primary")
                onTriggered: root.account.isDefaultWallet = !root.account.isDefaultWallet
            }
        property QtObject encryptAction: Action {
                text: qsTr("Protect With Pin...")
                onTriggered: {
                    portfolio.current = root.account;
                    accountOverlay.state = "startWalletEncryption";
                }
            }
        property QtObject openWalletAction: Action {
                text: qsTr("Open", "Open encrypted wallet")
                onTriggered: tabbar.currentIndex = 0
            }

        property QtObject closeWalletAction: Action {
                text: qsTr("Close", "Close encrypted wallet")
                onTriggered: root.account.closeWallet();
            }

        onAboutToOpen: {
            var items = [];
            items.push(detailsAction);
            var encrypted = root.account.needsPinToOpen;
            var decrypted = root.account.isDecrypted;
            if ((encrypted || root.account.needsPinToPay) && decrypted)
                items.push(closeWalletAction);
            if (encrypted && !decrypted && tabbar.currentIndex != 0)
                items.push(openWalletAction);
            var isArchived = root.account.isArchived;
            if (!isArchived)
                items.push(primaryAction);
            if (!encrypted)
                items.push(encryptAction);
            items.push(archiveAction);
            setMenuActions(items);
        }
    }
}
