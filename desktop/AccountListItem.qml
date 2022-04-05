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
import "widgets" as Flowee

Item {
    id: root
    property QtObject account: null
    height: column.height + 26
    property alias contextMenu: configItem.contextMenu
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
                text: Pay.formatDate(account.lastMinedTransaction)
            }
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
    }
}
