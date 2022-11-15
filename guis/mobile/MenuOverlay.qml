/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import "../ControlColors.js" as ControlColors

Item {
    id: root
    property bool open: false

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        opacity: root.open ? 0.5 : 0
        color: "black"
    }

    Rectangle {
        id: menuArea
        color: mainWindow.palette.base
        width: 300
        height: parent.height
        x: root.open ? 0 : 0 - width -3
        clip: true

        Rectangle {
            // TODO get little light-on/light-off icon instead
            color: "black"
            width: 20
            height: 20
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 10
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    Pay.useDarkSkin = !Pay.useDarkSkin
                    ControlColors.applySkin(mainWindow)
                }
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: 10
            y: 40
            x: 15
            Label {
                text: "Network Details"
                width: parent.width
                wrapMode: Text.WordWrap
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    onClicked: {
                        thePile.push("NetView.qml");
                        root.open = false;
                    }
                }
            }
        }

        Behavior on x { NumberAnimation { } }
    }
    // allow close by clicking next to the menu
    MouseArea {
        width: parent.width - menuArea.width
        height: parent.height
        anchors.right: parent.right
        enabled: root.open
        onClicked: root.open = false;
    }

    // TODO add gesture (swipe right) to close menu
}
