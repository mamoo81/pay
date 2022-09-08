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

ScrollBar {
    id: root

    /// override this if the flickable is not the direct parent.
    property var flickable: parent
    /// A component will be made visible when the user moves the thumb,
    /// allowing the showing of details based on the position
    property Component preview: Item {}

    background: Item {}
    contentItem: Item {}

    Item {
        parent: root.flickable
        anchors.fill: parent
        Loader {
            anchors.centerIn: parent
            sourceComponent: root.preview

            opacity: thumbInput.engaged
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.InQuad } }
        }
    }

    Rectangle {
        id: thumbRect
        property bool open: false
        property bool moving: root.active || thumbInput.engaged
        onMovingChanged: if (moving) open = true
        width: 18
        height: 30
        x: moving || open ? parent.width - width: parent.width + 2
        y: {
            var pos = root.position
            var size = root.size
            var viewHeight = root.flickable.height
            var newY = viewHeight * pos + viewHeight * size / 2 - height / 2
            if (newY < 0)
                return 0;
            var max = viewHeight - height;
            if (newY > max)
                return max;
            return newY;
        }

        Column {
            id: column
            spacing: 2
            anchors.centerIn: parent
            width: thumbRect.width - 6
            Repeater {
                model: 3
                delegate: Rectangle {
                    color: root.palette.light
                    width: column.width
                    height: 2
                    radius: 1
                }
            }
        }
        color: thumbInput.engaged ? root.palette.highlight : root.palette.dark

        Timer {
            running: thumbRect.open && !thumbRect.moving
            interval: 500
            onTriggered: thumbRect.open = false;
        }

        Behavior on x {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InQuad
            }
        }
    }

    MouseArea {
        id: thumbInput
        // make it easier to grab by having a bigger mouse area than the visial thumb
        width: thumbRect.width + 20 + root.width
        height: thumbRect.height + 20
        anchors.right: parent.right
        enabled: thumbRect.moving || thumbRect.open
        y: {
            var pos = root.position
            var size = root.size
            var viewHeight = root.flickable.height
            return viewHeight * pos + viewHeight * size / 2 - height / 2
        }

        property int startY: 0
        property real startPos: 0
        property bool engaged: false // seems that 'Mousearea.pressed' behaves different than I expect, this works better
        onPressed: (mouse)=> {
            startY = root.flickable.mapFromItem(thumbInput, mouse.x, mouse.y).y
            startPos = root.position
            engaged = true
        }
        onReleased: engaged = false
        preventStealing: true
        onPositionChanged: (mouse)=> {
            // Most of the scroller properties are in the 0.0 - 1.0 range
            var absolutePos = root.flickable.mapFromItem(thumbInput, mouse.x, mouse.y);
            var diff = startY - absolutePos.y;
            var viewHeight = root.flickable.height
            var moved = diff / viewHeight;
            var newPos = startPos - moved;
            if (newPos < 0)
                newPos = 0;
            var max = 1 - root.size;
            if (newPos > max)
                newPos = max;
            root.position = newPos
        }
    }
}
