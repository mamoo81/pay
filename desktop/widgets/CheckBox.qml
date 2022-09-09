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

Item {
    id: root
    property bool checked: false
    property bool sliderOnIndicator: true
    property alias text: title.text
    property string tooltipText: ""
    signal clicked;
    implicitWidth: slider.width + 6 + title.width + (tooltipText === "" ? 0 : (questionMarkIcon.width + 16))
    implicitHeight: Math.max(slider.implicitHeight, title.implicitHeight)
    clip: true
    activeFocusOnTab: true

    Item {
        id: slider
        width: implicitWidth
        height: implicitHeight
        implicitWidth: 50
        implicitHeight: 22

        Rectangle {
            anchors.fill: parent
            radius: parent.height / 3
            color: root.sliderOnIndicator && root.enabled && root.checked ? (Pay.useDarkSkin ? "#4f7d63" : "#9ec7af") : mainWindow.palette.window
            border.color: root.activeFocus ? mainWindow.palette.highlight : mainWindow.palette.button
            border.width: 0.8

            Behavior on color { ColorAnimation {}}
        }
        Rectangle {
            width: parent.height / 5 * 4
            height: width
            radius: width
            x: root.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            color: {
                if (!root.enabled)
                    return "darkgray"
                if (root.checked && Pay.useDarkSkin)
                    return mainWindow.palette.text;
                return mainWindow.palette.highlight
            }
            Behavior on x { NumberAnimation {}}
            Behavior on color { ColorAnimation {}}
        }
    }

    MouseArea {
        id: mousy
        width: slider.width + 6 + title.width
        height: parent.height
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.clicked()
        }
        hoverEnabled: true

        ToolTip {
            parent: root
            text: root.tooltipText
            delay: 1500
            visible: mousy.containsMouse && root.tooltipText !== ""
        }
    }
    Label {
        id: title
        anchors.left: slider.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 6
        color: enabled ? palette.text : "darkgray"
    }

    Rectangle {
        id: questionMarkIcon
        visible: root.tooltipText !== "" && root.enabled
        width: q.width + 14
        height: width
        anchors.left: title.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        radius: width
        color: q.palette.text
        Label {
            id: q
            text: "?"
            color: palette.base
            anchors.centerIn: parent
            MouseArea {
                anchors.fill: parent
                anchors.margins: -7
                cursorShape: Qt.PointingHandCursor
                id: clicky
                onClicked: ToolTip.show(root.tooltipText, 15000);
            }
        }
    }
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            root.checked = !root.checked
            event.accepted = true
        }
        else if (root.checked && event.key === Qt.Key_Left) {
            root.checked = false;
            event.accepted = true
        }
        else if (!root.checked && event.key === Qt.Key_Right) {
            root.checked = true;
            event.accepted = true
        }
    }
}
