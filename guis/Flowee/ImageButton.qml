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

QQC2.Control {
    width: 42
    height: 42

    signal clicked;
    property alias source: imageIcon.source;
    property alias responseText: comment.text;

    Rectangle {
        id: highlight
        anchors.fill: parent
        color: palette.button
        visible: mouseArea.containsMouse
        radius: 6
    }
    Image {
        id: imageIcon
        anchors.fill: parent
        anchors.margins: 4
        smooth: true
    }
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            parent.clicked()
            if (comment.text !== "")
                feedbackPopup.visible = true
        }
    }

    Rectangle {
        id: feedbackPopup
        radius: 6
        color: palette.window
        border.width: 2
        border.color: palette.highlight
        visible: false
        width: comment.width + 12
        height: comment.height + 12
        anchors.bottom: parent.top
        anchors.horizontalCenter: parent.horizontalCenter

        Timer {
            interval: 3000
            running: parent.visible
            onTriggered: parent.visible = false
        }
        Label {
            id: comment
            anchors.centerIn: parent
        }
    }
}
