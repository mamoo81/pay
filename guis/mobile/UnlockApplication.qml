/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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

FocusScope {
    Rectangle {
        anchors.fill: parent
        color: palette.window
    }
    MouseArea { // eat all mouse events.
        anchors.fill: parent
    }

    UnlockWidget {
        anchors.fill: parent
        anchors.margins: 10
        onPasswordEntered: if (!Pay.checkAppPassword(password)) passwordIncorrect();

        // called from main.qml, need to be implemented here
        // since we dont extend the Page type.
        function takeFocus() {
        }
    }
    Keys.onPressed: (event)=> {
        event.accepted = true; // at all key events.
    }
}
