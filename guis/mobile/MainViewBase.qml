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
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee

QQC2.Control {
    id: root
    width: parent.width
    height: parent.height

    // This trick  means any child items are actually added to the 'stack' item's children.
    default property alias content: stack.children
    property int currentIndex: 0
    onCurrentIndexChanged: setOpacities()
    Component.onCompleted: setOpacities()

    function setOpacities() {
        for (let i = 0; i < stack.children.length; ++i) {
            let on = i === currentIndex;
            let child = stack.children[i];
            child.visible = on;
        }
        takeFocus();
    }
    function switchToTab(index) {
        currentIndex = index
    }

    // called from main when this page becomes active, as well as when we change tabs
    function takeFocus() {
        // try to make sure the current tab gets keyboard focus properly.
        // We do some extra work in case the tab itself is a loader.
        forceActiveFocus();
        let visibleChild = stack.children[currentIndex];
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
        height: 50
        color: Pay.useDarkSkin ? root.palette.base : mainWindow.floweeBlue

        Column {
            id: menuButton
            spacing: 3
            y: 12
            x: 10

            Repeater {
                model: 3
                delegate: Rectangle {
                    color: "white"
                    width: 12
                    height: 3
                    radius: 2
                }
            }
        }
        MouseArea {
            anchors.fill: menuButton
            anchors.margins: -20
            cursorShape: Qt.PointingHandCursor
            onClicked: menuOverlay.open = true;
        }

        Item {
            // Here we just want the text part. So clip that out.
            clip: true
            y: 10
            x: 32
            width: 122
            height: 21
            Image {
                source: "qrc:/FloweePay.svg"
                // ratio: 449 / 77
                width: 150
                height: 26
                x: -28
                y: -5
            }
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
            delegate: Rectangle {
                height: 80
                width: root.width / stack.children.length;
                color: {
                    modelData === root.currentIndex
                        ? root.palette.button
                        : root.palette.base
                }
                Image {
                    source: stack.children[modelData].icon
                    width: 35
                    height: 35
                    smooth: true
                    y: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Flowee.Label {
                    text: stack.children[modelData].title
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.currentIndex = modelData
                }
            }
        }
    }
}
