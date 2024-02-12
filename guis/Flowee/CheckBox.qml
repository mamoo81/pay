/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2024 Tom Zander <tom@flowee.org>
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

    implicitWidth: indicator.width + contentItem.implicitWidth + (toolTipText === "" ? 0 : (questionMarkIcon.width + 10))
    implicitHeight: Math.max(indicator.implicitHeight, contentItem.implicitHeight)
    clip: true
    spacing: 6

    indicator: Item {
        implicitHeight: titleLabel.font.pixelSize
        implicitWidth: implicitHeight * 2.1
        y: 4.5 // make the inner circle visually baseline aligned

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
    contentItem: Label {
        id: titleLabel
        text: control.text
        opacity: enabled ? 1.0 : 0.7
        anchors.verticalCenter: parent.verticalCenter
        color: enabled ? palette.windowText : "darkgray"
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        leftPadding: text === "" ? 0 : (control.indicator.width + control.spacing)
    }

    Rectangle {
        id: questionMarkIcon
        visible: control.toolTipText !== "" && control.enabled
        width: q.implicitWidth + 14
        height: q.implicitHeight
        x: titleLabel.x + titleLabel.implicitWidth + 10
        anchors.verticalCenter: contentItem.verticalCenter
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
                onClicked: QQC2.ToolTip.show(control.toolTipText, 15000);
            }
        }
    }
}
