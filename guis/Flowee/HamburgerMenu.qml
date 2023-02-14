/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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
    implicitWidth: root.wide ? 12 : 4
    implicitHeight: 16
    property bool wide: false
    property color color: mainWindow.palette.windowText

    Column {
        spacing: 3
        y: 1 // move the column down to account for the anti-alias line of the rectangle below
        Repeater {
            model: 3
            delegate: Rectangle {
                id: rect
                color: root.color
                width: root.wide ? 12 : 4
                height: 3
                radius: 2
            }
        }
    }
}
