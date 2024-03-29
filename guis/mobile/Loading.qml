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

Rectangle {
    color: "#0b1088"
    width: parent.width
    height: parent.height
    function takeFocus() {
        // method called from main.qml
        // We don't need focus, though. Empty method.
    }

    Image {
        source: "qrc:/FloweePay-light.svg"
        // ratio: 449 / 77
        width: parent.width / 10 * 8
        height: width / 449 * 77
        anchors.centerIn: parent
    }
}
