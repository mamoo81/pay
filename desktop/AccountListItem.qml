/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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

Item {
    id: root
    property QtObject account: null
    height: column.height + 26
    visible: account.isUserOwned

    Column {
        id: column
        // spacing: 4
        x: 11
        y: 11
        width: parent.width
        /*
        Label {
            text: "Savings Account"
            opacity: 0.6
        }*/
        Label {
            x: 20
            text: root.account.name
            font.bold: true
            width: parent.width - 55
            clip: true
        }
        Flow {
            width: parent.width - 20
            x: 20
            spacing: 10
            visible: lastReceive.text !== ""
            Label {
                text: qsTr("Last Transaction") + ":"
            }
            Label {
                id: lastReceive
                text: Flowee.formatDate(account.lastMinedTransaction)
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 5
        id: background
        property bool hover: false
        radius: 10
        color: "#00000000" // transparant
        border.width: 3
        border.color: {
            if (portfolio.current === account)
                return mainWindow.floweeGreen
            if (hover)
                return mainWindow.floweeSalmon;
            return Flowee.useDarkSkin ? "#EEE" : mainWindow.floweeBlue
        }
    }

    ArrowPoint {
        id: point
        x: 5
        visible: portfolio.current === account;
        anchors.verticalCenter: parent.verticalCenter
        color: background.border.color
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: portfolio.current = account
        onEntered: background.hover = true
        onExited: background.hover = false
    }

    ConfigItem {
        anchors.right: parent.right
        anchors.rightMargin: 15
        y: 13

        contextMenu:  Menu {
            MenuItem {
                text: qsTr("Details")
                onTriggered: {
                    portfolio.current = account;
                    accountDetails.state = "accountDetails";
                }
            }
            MenuItem {
                checked: root.account.isDefaultWallet
                checkable: true
                text: qsTr("Main Account")
                onTriggered: root.account.isDefaultWallet = !root.account.isDefaultWallet
            }
        }
    }
}
