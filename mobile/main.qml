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

ApplicationWindow {
    id: mainWindow
    title: "Flowee Pay"
    width: 360
    height: 720
    visible: true

    property bool isLoading: typeof portfolio === "undefined";

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"

    Rectangle {
        focus: true
        id: header
        color: Pay.useDarkSkin ? "#232629" : mainWindow.floweeBlue
        anchors.fill: parent

        Image {
            id: appLogo
            x: 10
            y: 10
            smooth: true
            source: "FloweePay-light.svg"
            // ratio: 77 / 449
            height: 50
            width: height * 449 / 77
        }
    }
}
