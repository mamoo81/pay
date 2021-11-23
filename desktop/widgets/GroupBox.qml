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
import QtQuick.Layouts 1.11

Item {
    id: root

    property bool collapsable: true
    property bool collapsed: false
    property alias title: titleLabel.text
    property alias summary: summaryLabel.text
    default property alias content: child.children
    property alias columns: child.columns
    activeFocusOnTab: collapsable
    clip: true

    width: 200 // should be changed by user
    implicitHeight: arrowPoint.height + (collapsed ? 0 : child.implicitHeight + 25) // 25 = 15 top, 5 bottom of content area

    Rectangle { // the outline
        width: parent.width
        y: titleArea.visible ? titleLabel.height / 2 : arrowPoint.height / 2
        height: root.collapsed ? 1 : parent.height - y;
        color: "#00000000"
        border.color: titleLabel.palette.button
        border.width: 2
        radius: 3
    }
    MouseArea {
        width: parent.width
        height: 20
        enabled: root.collapsable
        visible: enabled // this line is needed to make the cursor shape not be set when not enabled (Qt bug)
        onClicked: root.collapsed = !root.collapsed
        cursorShape: Qt.PointingHandCursor
    }

    Rectangle {
        id: titleArea
        visible: titleLabel.text !== ""
        color: titleLabel.palette.window
        width: titleLabel.width + 18 + (summaryLabel.visible ? summaryLabel.width + 20 : 0)
        height: titleLabel.height
        anchors.left: arrowPoint.right
        Label {
            id: titleLabel
            x: 12

            // focus indicator
            Rectangle {
               anchors.fill: parent
               anchors.leftMargin: -2
               anchors.rightMargin: -2
               color: "#00000000"
               border.color: parent.palette.highlight
               border.width: 2
               visible: root.activeFocus
            }
        }

        Label {
            id: summaryLabel
            anchors.left: titleLabel.right
            anchors.leftMargin: 20
            visible: root.collapsed && text !== ""
            font.italic: true
        }
    }

    Rectangle {
        id: arrowPoint
        visible: parent.collapsable
        color: titleLabel.palette.window
        width: point.width + 12
        height: point.height + 2
        x: 6
        y: 2
        ArrowPoint {
            id: point
            x: 10
            color: Pay.useDarkSkin ? "white" : "black"
            rotation: root.collapsed ? 0 : 90
            transformOrigin: Item.Center

            Behavior on rotation { NumberAnimation {} }
        }
    }

    GridLayout { // user set content
        id: child
        x: 10
        y: titleArea.height + 10
        width: root.width - 20
        height: implicitHeight
        visible: !root.collapsed
        columns: 1
    }

    Behavior on implicitHeight { NumberAnimation { } }
    Keys.onPressed: {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            if (root.collapsable) {
                root.collapsed = !root.collapsed
                event.accepted = true
            }
        }
    }
}
