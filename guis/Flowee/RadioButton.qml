/*
 * This file is part of the Flowee project
 * Copyright (C) 2023-2024 Tom Zander <tom@flowee.org>
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

T.RadioButton {
    id: control

    implicitWidth: 140
    implicitHeight: contentItem.contentHeight + 10
    spacing: 6

    contentItem: Label {
        id: textLabel
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.down ? palette.midlight : palette.text
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    indicator: Rectangle {
        implicitWidth: textLabel.font.pixelSize
        implicitHeight: implicitWidth
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: implicitWidth / 2
        color: "#00000000" // transparant background
        border.color: palette.button

        Rectangle {
            width: parent.width * 0.8
            height: parent.height * 0.8
            anchors.centerIn: parent
            radius: width / 2
            color: palette.highlight
            visible: control.checked
        }
    }
}
