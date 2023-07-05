/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2023 Tom Zander <tom@flowee.org>
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
    property string toolTipText: ""

    implicitWidth: slider.width + 6 + title.implicitWidth + (toolTipText === "" ? 0 : (questionMarkIcon.width + 16))
    implicitHeight: Math.max(slider.implicitHeight, title.implicitHeight)
    clip: true

    indicator: Item {
        id: slider
        implicitWidth: implicitHeight * 2.1
        implicitHeight: title.implicitHeight / title.lineCount

        Rectangle {
            anchors.fill: parent
            radius: parent.height / 3
            color: {
                if (control.sliderOnIndicator && control.enabled && control.checked)
                    return palette.midlight
                return palette.window
            }
            border.color: control.activeFocus ? palette.highlight : palette.button
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
                    return palette.windowText;
                return palette.highlight
            }
            Behavior on x { NumberAnimation {}}
            Behavior on color { ColorAnimation {}}
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.margins: -5 // make it more finger friendly and assume a 10 pixel gap between elements.
        onClicked: {
            control.toggle()
            control.clicked();
            control.toggled();
        }
        cursorShape: Qt.PointingHandCursor
    }

    Label {
        id: title
        text: control.text
        anchors.left: slider.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 6
        anchors.right: parent.right
        color: enabled ? palette.windowText : "darkgray"
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Rectangle {
        id: questionMarkIcon
        visible: control.toolTipText !== "" && control.enabled
        width: title.implicitWidth + 14
        height: title.implicitHeight
        anchors.left: title.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        radius: width
        color: palette.windowText
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
                onClicked: QQC2.ToolTip.show(control.toolTipText, 15000);
            }
        }
    }
}
