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

Item {
    id: warningLabel
    property alias text: warningText.text
    property alias color: warningText.color

    visible: warningText.text !== ""
    height: Math.max(warningIcon.height, warningText.height)
    implicitWidth: warningIcon.width + warningText.implicitWidth
    implicitHeight: Math.max(warningIcon.height, warningText.implicitHeight)
    Image {
        id: warningIcon
        source: "qrc:/emblem-warning.svg"
        smooth: true
        width: 32
        height: 32
        anchors.verticalCenter: parent.verticalCenter
    }
    Label {
        id: warningText
        anchors.left: warningIcon.right
        anchors.leftMargin: 5
        anchors.verticalCenter: warningIcon.verticalCenter
        anchors.right: parent.right
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }
}
