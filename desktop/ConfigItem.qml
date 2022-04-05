/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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

Item {
    id: root
    width: wide ? 10 : 4
    height: column.height
    property color color: Pay.useDarkSkin ? "white" : "black"
    property bool wide: false

    property Component contextMenu: Component { Item {} }

    Column {
        id: column
        spacing: 3
        y: 1 // move the column down to account for the anti-alias line of the rectangle below

        Repeater {
            model: 3
            delegate: Rectangle {
                color: root.color
                width: root.wide ? 14 : 3
                height: 3
                radius: 3
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        anchors.margins: -15
        hoverEnabled: true // to make sure we eat them and avoid the hover feedback.
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        cursorShape: Qt.PointingHandCursor

        Loader { id: contextMenu }
        onClicked: {
            if (contextMenu.item == null) {
                contextMenu.sourceComponent = root.contextMenu
            }
            // if there is no contextMenu set we just get an error printed here. No problem
            contextMenu.item.popup()
        }
    }
}
