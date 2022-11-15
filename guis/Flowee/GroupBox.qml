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
import QtQuick.Controls 2.11 as QQC2
import QtQuick.Layouts 1.11

QQC2.Control {
    id: root

    property bool collapsable: true
    property bool collapsed: false
    /**
     * This is like collapsed, but when the user changes the groupbox collapse-state, this is updated.
     * We never overwrite the 'collapsed' property to enable binding it to a model, this is the effective
     * collapsed state taking both changes from the `collapsed` property and from user input.
     */
    property bool effectiveCollapsed: collapsed
    property alias title: titleLabel.text
    property alias summary: summaryLabel.text
    default property alias content: child.children
    property alias columns: child.columns
    activeFocusOnTab: collapsable
    clip: true

    onCollapsedChanged: effectiveCollapsed = collapsed

    width: 200 // should be changed by user
    implicitHeight: Math.max(titleArea.height, arrowPoint.height) + (effectiveCollapsed ? 0 : child.implicitHeight + 25)

    Rectangle { // the outline
        width: parent.width
        y: titleArea.visible ? titleLabel.height / 2 : arrowPoint.height / 2
        height: root.effectiveCollapsed ? 1 : parent.height - y;
        color: "#00000000"
        border.color: titleLabel.palette.button
        border.width: 1.3
        radius: 3
    }
    MouseArea {
        width: parent.width
        height: 20
        enabled: root.collapsable
        visible: enabled // this line is needed to make the cursor shape not be set when not enabled (Qt bug)
        onClicked: root.effectiveCollapsed = !root.effectiveCollapsed
        cursorShape: Qt.PointingHandCursor
    }

    Rectangle {
        id: titleArea
        visible: titleLabel.text !== ""
        color: titleLabel.palette.window
        width: titleLabel.width + 6 + (summaryLabel.visible ? summaryLabel.width + 6 : 0)
        height: titleLabel.height
        anchors.left: arrowPoint.right
        Text {
            id: titleLabel
            x: 3
            color: root.palette.text

            // focus indicator
            Rectangle {
               anchors.fill: parent
               anchors.leftMargin: -2
               anchors.rightMargin: -2
               color: "#00000000"
               border.color: parent.palette.highlight
               border.width: 0.8
               visible: root.activeFocus
            }
        }

        Text {
            id: summaryLabel
            anchors.left: titleLabel.right
            anchors.leftMargin: 20
            visible: root.effectiveCollapsed && text !== ""
            font.italic: true
            color: root.palette.text
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
            x: 6
            y: 1
            color: Pay.useDarkSkin ? "white" : "black"
            rotation: root.effectiveCollapsed ? 0 : 90
            transformOrigin: Item.Center

            Behavior on rotation { NumberAnimation {} }
        }
    }

    GridLayout { // user set content
        id: child
        x: 12
        y: titleArea.height + 10
        width: root.width - 24
        height: implicitHeight
        visible: !root.effectiveCollapsed
        columns: 1
    }

    Behavior on implicitHeight { NumberAnimation { } }
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            if (root.collapsable) {
                root.effectiveCollapsed = !root.effectiveCollapsed
                event.accepted = true
            }
        }
    }
}
