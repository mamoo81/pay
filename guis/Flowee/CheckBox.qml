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
import QtQuick.Controls as QQC2
import QtQuick.Templates as T

T.CheckBox {
    id: control

    property bool sliderOnIndicator: true
    property string tooltipText: ""

    implicitWidth: slider.width + 6 + title.width + (tooltipText === "" ? 0 : (questionMarkIcon.width + 16))
    implicitHeight: Math.max(slider.implicitHeight, title.implicitHeight)
    clip: true

    indicator: Item {
        id: slider
        width: implicitWidth
        height: implicitHeight
        implicitWidth: 50
        implicitHeight: 22

        Rectangle {
            anchors.fill: parent
            radius: parent.height / 3
            color: control.sliderOnIndicator && control.enabled && control.checked ? (Pay.useDarkSkin ? "#4f7d63" : "#9ec7af") : mainWindow.palette.window
            border.color: control.activeFocus ? mainWindow.palette.highlight : mainWindow.palette.button
            border.width: 0.8

            Behavior on color { ColorAnimation {}}
        }
        Rectangle {
            width: parent.height / 5 * 4
            height: width
            radius: width
            x: control.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            color: {
                if (!control.enabled)
                    return "darkgray"
                if (control.checked && Pay.useDarkSkin)
                    return mainWindow.palette.windowText;
                return mainWindow.palette.highlight
            }
            Behavior on x { NumberAnimation {}}
            Behavior on color { ColorAnimation {}}
        }
    }

    Label {
        id: title
        text: control.text
        anchors.left: slider.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 6
        color: enabled ? palette.windowText : "darkgray"
    }

    Rectangle {
        id: questionMarkIcon
        visible: control.tooltipText !== "" && control.enabled
        width: q.width + 14
        height: width
        anchors.left: title.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        radius: width
        color: q.palette.windowText
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
                onClicked: QQC2.ToolTip.show(control.tooltipText, 15000);
            }
        }
    }
}
