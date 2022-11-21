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
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 2.15
import "../ControlColors.js" as ControlColors
import "../Flowee" as Flowee

Item {
    id: root
    property bool open: false

    Rectangle {
        anchors.fill: parent
        opacity: {
            if (!root.open)
                return 0;
            // we become 50% opaque when the menuArea is fully open (x == 0)
            return (menuArea.x + 250) / 500;
        }
        color: "black"
    }

    Rectangle {
        id: menuArea
        color: mainWindow.palette.base
        width: 300
        height: parent.height
        x: root.open ? 0 : 0 - width -3
        clip: true

        Image {
            width: 25
            height: 25
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 10
            source: Pay.useDarkSkin ? "qrc:/maslenica.svg" : "qrc:/moon.svg"
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
            Repeater {
                model: MenuModel

                Flowee.Label {
                    text: model.name
                    width: menuArea.width
                    wrapMode: Text.WordWrap
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        onClicked: {
                            var target = model.target
                            if (target !== "") {
                                thePile.push(model.target)
                                root.open = false;
                            }
                        }
                    }
                }
            }
        }

        Behavior on x { NumberAnimation { duration: 100 } }

        property bool opened: false
        onXChanged: {
            if (!root.open)
                opened = false;
            else if (x === 0)
                opened = true;
            // close on user drag to the left
            if (opened && x < -50)
                root.open = false
        }
        // gesture (swipe right) to close menu
        DragHandler {
            id: dragHandler
            enabled: root.open
            yAxis.enabled: false
            xAxis.minimum: -200
            xAxis.maximum: 0

            onActiveChanged: {
                // should the user abort the swipe left, restore
                // the original binding
                if (!active && root.open)
                    menuArea.x = root.open ? 0 : 0 - width -3
            }
        }
    }
    // allow close by clicking next to the menu
    MouseArea {
        width: parent.width - menuArea.width
        height: parent.height
        anchors.right: parent.right
        enabled: root.open
        onClicked: root.open = false;
    }

}
