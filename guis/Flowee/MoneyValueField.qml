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
import QtQuick.Controls 2.11 as QQC2
import Flowee.org.pay 1.0

/*
 * Similar to TextField, this is a wrapper around BitcoinAmountLabel
 * in order to provide a background and ediing capabilities.
 */
FocusScope {
    id: root
    activeFocusOnTab: true

    property alias value: privValue.value
    property alias valueObject: privValue

    signal valueEdited;

    onEnabledChanged: {
        if (!enabled)
            root.focus = false
    }

    function reset() {
        privValue.reset();
    }

    BitcoinValue {
        id: privValue
    }
    MouseArea {
        cursorShape: Qt.IBeamCursor
        anchors.fill: parent
        enabled: root.enabled
        onClicked: {
            root.focus = true
            root.forceActiveFocus()
        }
    }

    Rectangle { // focus indicator
        anchors.fill: parent
        border.color: root.activeFocus ? begin.palette.highlight  : begin.palette.mid
        border.width: 0.8
        color: root.enabled && !Pay.useDarkSkin ? "white" : "#00000000" // transparant
    }

    Item { // edit-label
        visible: root.activeFocus
        x: 8
        y: 6
        QQC2.Label {
            id: begin
            text: privValue.enteredString.substring(0, privValue.cursorPos)
        }
        Rectangle {
            id: cursor
            anchors.left: begin.right
            width: 1
            height: root.height - 12
            color: cursorVisible ? begin.palette.text : "#00000000"
            property bool cursorVisible: true
            Timer {
                id: blinkingCursor
                running: root.activeFocus
                repeat: true
                interval: 500
                onTriggered: parent.cursorVisible = !parent.cursorVisible
            }
        }
        QQC2.Label {
            anchors.left: begin.right
            text: privValue.enteredString.substring(privValue.cursorPos)
        }
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
            blinkingCursor.restart();
            cursor.cursorVisible = true;
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Right) {
            privValue.moveRight();
            cursor.cursorVisible = true;
            blinkingCursor.restart();
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
