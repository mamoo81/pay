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
import QtQuick 2.14

Item {
    id: arrowPoint
    property alias color: rotatedRect.color

    clip: true
    Rectangle {
        id: rotatedRect
        width: parent.height / 1.41
        height: width
        rotation: 45
        transformOrigin: Item.Center
        x: (width - parent.width) / 2 - (width / 2 * 1.41) + 2
        y: (height - parent.width) / 2
    }
}
