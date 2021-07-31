/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay 1.0

Item {
    id: root
    width: wide ? 14 : 3
    height: column.height
    property color color: Flowee.useDarkSkin ? "white" : "black"
    property bool wide: false
    Column {
        id: column
        spacing: 3

        Repeater {
            model: 4
            delegate: Rectangle {
                color: root.color
                width: root.wide ? 14 : 3
                height: 3
                radius: 3
            }
        }
    }
}
