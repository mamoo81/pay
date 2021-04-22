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
import QtQuick 2.14

Item {
    id: floweeCheckBox
    property bool checked: false
    signal clicked;

    width: implicitWidth
    height: implicitHeight
    implicitWidth: 60
    implicitHeight: 27

    Rectangle {
        anchors.fill: parent
        radius: parent.height / 3
        color: mainWindow.palette.window
        border.color: mainWindow.palette.button
        border.width: 2

    }
    Rectangle {
        width: parent.height / 5 * 4
        height: width
        radius: width
        x: parent.checked ? parent.width - width - 3 : 3
        anchors.verticalCenter: parent.verticalCenter
        color: mainWindow.palette.highlight

        Behavior on x { NumberAnimation {}}
    }

    MouseArea {
        anchors.fill: parent
        onClicked: parent.clicked()
    }
}
