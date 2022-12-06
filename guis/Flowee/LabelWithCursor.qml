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

Item {
    id: root

    property alias fontPixelSize: begin.font.pixelSize
    property alias color: begin.color
    property alias textOpacity: begin.opacity
    property string fullString: ""
    property int startPos: 0
    property int stringLength: fullString.length
    property int cursorPos: 0
    property bool showCursor: false
    implicitWidth: begin.implicitWidth + secondPart.implicitWidth + (cursor.visible ? 1 : 0)
    implicitHeight: begin.height
    width: implicitWidth
    height: implicitHeight

    property string text: {
        return fullString.substring(startPos, startPos + stringLength)
    }

    baselineOffset: begin.baselineOffset

    Label {
        id: begin
        color: palette.text
        text: {
            var fullText = root.text
            var cutPoint = parent.cursorPos - parent.startPos;
            if (cutPoint >= 0 && cutPoint < parent.stringLength) {
                return fullText.substring(0, cutPoint);
            }
            return fullText;
        }
    }
    Rectangle {
        id: cursor
        anchors.left: begin.right
        width: 1.3
        height: root.height
        visible: {
            var cutPoint = parent.cursorPos - parent.startPos;
            var lastSegment = parent.fullString.length === parent.startPos + parent.stringLength
            if (cutPoint < 0)
                return false;
            if (lastSegment)
                return cutPoint <= parent.stringLength
            return cutPoint < parent.stringLength
        }
        color: cursorVisible ? begin.palette.text : "#00000000"
        property bool cursorVisible: true
        Timer {
            id: blinkingCursor
            running: root.showCursor
            repeat: true
            interval: 500
            onTriggered: parent.cursorVisible = !parent.cursorVisible
        }
    }
    Label {
        id: secondPart
        color: begin.color
        anchors.left: begin.right
        font.pixelSize: begin.font.pixelSize
        opacity: begin.opacity
        text: {
            var cutPoint = parent.cursorPos - parent.startPos;
            if (cutPoint >= 0 && cutPoint < parent.stringLength) {
                return root.text.substring(cutPoint);
            }
            return "";
        }
    }
}
