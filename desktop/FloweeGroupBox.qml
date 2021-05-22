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

import Flowee.org.pay 1.0

Item {
    id: root

    property bool collapsable: true
    property bool isCollapsed: false
    property string title: ""
    property alias content: child.children
    property alias columns: child.columns
    clip: true

    width: 100 // should be changed by user
    height: implicitHeight
    implicitWidth: {
        var w = child.width // even if its hidden, it takes horizontal space
        if (titleArea.visible) {
            var headerWidth = titleArea.width
            if (arrowPoint.visible)
                headerWidth += arrowPoint.width + 10
        }
        return w;
    }
    implicitHeight: arrowPoint.height + (isCollapsed ? 0 : child.height + 10)

    Rectangle { // the outline
        width: parent.width
        y: titleArea.visible ? title.height / 2 : arrowPoint.height / 2
        height: root.isCollapsed ? 1 : parent.height - y;
        color: "#00000000"
        border.color: title.palette.button
        border.width: 2
        radius: 3
    }
    MouseArea {
        width: parent.width
        height: 20
        enabled: root.collapsable
        onClicked:  root.isCollapsed = !root.isCollapsed
    }

    Rectangle {
        id: titleArea
        visible: title.text !== ""
        color: title.palette.window
        width: title.width + 18
        height: title.height
        anchors.left: arrowPoint.right
        Label {
            id: title
            x: 12
            text: root.title
        }
    }

    Rectangle {
        id: arrowPoint
        visible: parent.collapsable
        color: title.palette.window
        width: point.width + 12
        height: point.height + 2
        x: 6
        y: 2
        ArrowPoint {
            id: point
            x: 10
            color: Flowee.useDarkSkin ? "white" : "black"
            rotation: root.isCollapsed ? 0 : 90
            transformOrigin: Item.Center

            Behavior on rotation { NumberAnimation {} }
        }
    }

    GridLayout { // user set content
        id: child
        x: 6
        y: titleArea.height
        width: root.width - 12
        height: implicitHeight
        visible: !root.isCollapsed
    }

    Behavior on height { NumberAnimation { } }
}
