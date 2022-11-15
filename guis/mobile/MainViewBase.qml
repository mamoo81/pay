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
import "../Flowee" as Flowee

QQC2.Control {
    id: root
    width: parent.width
    height: parent.height

    // This trick  means any child items the FloweeTabBar are actually added to the 'stack' item's children.
    default property alias content: stack.children
    property int currentIndex: 0
    onCurrentIndexChanged: setOpacities()
    Component.onCompleted: setOpacities()

    function setOpacities() {
        var visibleChild = null;
        for (let i = 0; i < stack.children.length; ++i) {
            let on = i === currentIndex;
            let child = stack.children[i];
            child.visible = on;
            if (on)
                visibleChild = child;
        }

        // try to make sure the current tab gets keyboard focus properly.
        // We do some extra work in case the tab itself is a loader.
        forceActiveFocus();
        visibleChild.focus = true
        if (visibleChild instanceof Loader) {
            var child = visibleChild.item;
            if (child !== null)
                child.focus = true;
        }
    }

    Rectangle {
        id: header
        width: parent.width
        height: 40
        color: root.palette.base

        Column {
            id: menuButton
            spacing: 3
            y: 6
            x: 5

            Repeater {
                model: 3
                delegate: Rectangle {
                    color: mainWindow.palette.text
                    width: 12
                    height: 3
                    radius: 2
                }
            }
        }
        MouseArea {
            anchors.fill: menuButton
            anchors.margins: -10
            cursorShape: Qt.PointingHandCursor
            onClicked: menuOverlay.open = true;
        }

        Image {
            source: Pay.useDarkSkin ? "qrc:/FloweePay-light.svg" : "qrc:/FloweePay.svg"
            // ratio: 449 / 77
            width: 150
            height: 26
            y: 4
            x: 32
        }

    }

    Item {
        id: stack
        width: root.width
        anchors.top: header.bottom; anchors.bottom: tabbar.top
    }
    Row {
        id: tabbar
        anchors.bottom: parent.bottom

        Repeater {
            model: stack.children.length
            delegate: Item {
                height: 80
                width: root.width / stack.children.length;
                Flowee.Label {
                    text: modelData + 1
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.currentIndex = modelData
                }
            }
        }
    }
}
