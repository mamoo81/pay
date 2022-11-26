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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Flowee" as Flowee

Item {
    id: root
    property QtObject account: null
    height: column.height + 18
    signal clicked;

    //background
    Rectangle {
        property bool selected: portfolio.current === account

        anchors.fill: parent
        anchors.bottomMargin: 6
        id: background
        property bool hover: false
        radius: 7
        color: selected && !Pay.useDarkSkin ? "white" : "#00000000" // transparant
        border.width: 1.5
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
        spacing: 3
        x: 20
        y: 6
        width: parent.width // - 13
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
        width: 7
        height: 7
        radius: 7
        x: 12
        anchors.verticalCenter: parent.verticalCenter

        visible: {
            if (portfolio.current === root.account)
                return false;

            return root.account.hasFreshTransactions
        }
    }

    Flowee.ArrowPoint {
        id: point
        x: 1.3
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

    AccountConfigMenu {
        id: accountConfigMenu
        anchors.right: parent.right
        anchors.rightMargin: 6
        y: 6
        account: root.account
    }
}
