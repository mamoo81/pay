/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay 1.0

Item {
    id: root

    property var listView: parent
    property var scroller: null
    property real position: 0
    property alias hint: label.text
    visible: thumbRect.opacity != 0

    Rectangle {
        id: thumbRect
        opacity: root.scroller.active || thumbInput.engaged ? 1 : 0
        width: label.width + 30
        height: label.height + 40
        anchors.right: parent.right
        radius: 5
        y: {
            var pos = root.scroller.position
            var size = root.scroller.size
            var viewHeight = root.listView.height
            var newY = viewHeight * pos + viewHeight * size / 2 - height / 2
            if (newY < 0)
                return 0;
            var max = viewHeight - height;
            if (newY > max)
                return max;
            return newY;
        }

        color: label.palette.text

        Label {
            id: label
            x: 10
            y: 20
            color: label.palette.window
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 500
                easing.type: Easing.InQuad
            }
        }
    }

    MouseArea {
        id: thumbInput
        width: label.width + 30
        height: label.height + 40
        anchors.right: parent.right
        y: {
            var pos = root.scroller.position
            var size = root.scroller.size
            var viewHeight = root.listView.height
            return viewHeight * pos + viewHeight * size / 2 - height / 2
        }

        property int startY: 0
        property real startPos: 0
        property bool engaged: false // seems that 'Mousearea.pressed' behaves different than I expect, this works better
        onPressed: {
            startY = root.listView.mapFromItem(thumbInput, mouse.x, mouse.y).y
            startPos = root.scroller.position
            engaged = true
        }
        onReleased: engaged = false
        preventStealing: true
        onPositionChanged: {
            // Most of the scroller properties are in the 0.0 - 1.0 range
            var absolutePos = root.listView.mapFromItem(thumbInput, mouse.x, mouse.y);
            var diff = startY - absolutePos.y;
            var viewHeight = root.listView.height
            var moved = diff / viewHeight;
            var newPos = startPos - moved;
            if (newPos < 0)
                newPos = 0;
            var max = 1 - root.scroller.visualSize;
            if (newPos > max)
                newPos = max;
            root.position = newPos
            root.scroller.position = newPos
        }
    }
}
