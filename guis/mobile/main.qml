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
import QtQuick.Controls
import QtQuick.Layouts
import "../ControlColors.js" as ControlColors

ApplicationWindow {
    id: mainWindow
    title: "Flowee Pay"
    width: 360
    height: 720
    visible: true
    onVisibleChanged: if (visible) ControlColors.applySkin(mainWindow)

    property bool isLoading: typeof portfolio === "undefined";
    onIsLoadingChanged: {
        if (!isLoading) {
        }
    }

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"

    Item {
        id: header
        
    }

    StackView {
        id: stackView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
    }

    
}
