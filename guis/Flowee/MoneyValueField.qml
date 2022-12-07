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
import QtQuick
import Flowee.org.pay

FocusScope {
    id: root
    focus: true
    activeFocusOnTab: true
    property alias value: privValue.value
    property alias cursorPos: privValue.cursorPos
    property alias maxFractionalDigits: privValue.maxFractionalDigits
    signal valueEdited;

    // provide nested access to the BitcoinValue
    // this allows us to wrap methods
    property Item money: Item {
        property alias enteredString: privValue.enteredString
        function insertNumber(character) {
            let rc = privValue.insertNumber(character);
            if (rc)
                root.valueEdited();
            return rc;
        }
        function addSeparator() {
            let rc = privValue.addSeparator();
            if (rc)
                root.valueEdited();
            return rc;
        }
        function backspacePressed() {
            let rc = privValue.backspacePressed();
            if (rc)
                root.valueEdited();
            return rc;
        }
    }

    BitcoinValue {
        id: privValue
    }
    MouseArea {
        anchors.fill: parent
        onClicked: root.forceActiveFocus();
    }
    Rectangle {
        anchors.bottom: root.bottom
        anchors.left: root.left
        anchors.right: root.right
        height: 1.3
        color: mainWindow.palette.highlight
        visible: root.activeFocus
    }
    Keys.onPressed: (event)=> {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            privValue.insertNumber(event.key);
            event.accepted = true;
            root.valueEdited();
        }
        else if (event.key === 44 || event.key === 46) {
            privValue.addSeparator();
            event.accepted = true;
            root.valueEdited();
        }
        else if (event.key === Qt.Key_Backspace) {
            privValue.backspacePressed();
            event.accepted = true;
            root.valueEdited();
        }
        else if (event.key === Qt.Key_Left) {
            privValue.moveLeft();
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Right) {
            privValue.moveRight();
            event.accepted = true;
        }
    }
    Keys.onReleased: (event)=> {
        if (event.matches(StandardKey.Paste)) {
            privValue.paste();
            event.accepted = true;
            root.valueEdited();
        }
    }
}
