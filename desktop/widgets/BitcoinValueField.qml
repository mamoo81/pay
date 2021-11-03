/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay 1.0

/*
 * Similar to TextField, this is a wrapper around BitcoinAmountLabel
 * in order to provide a background and ediing capabilities.
 */
FocusScope {
    id: root
    implicitHeight: balance.height + 16
    implicitWidth: balance.width + 16
    activeFocusOnTab: true

    property alias value: privValue.value
    property alias valueObject: privValue
    property alias fontPtSize: balance.fontPtSize
    property bool enabled: true
    property double baselineOffset: balance.baselineOffset + balance.y

    signal valueEdited;

    onEnabledChanged: {
        if (!enabled)
            root.focus = false
    }

    function reset() {
        privValue.enteredString = "";
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

    BitcoinAmountLabel {
        id: balance
        x: 8
        y: 8
        value: root.value
        colorize: false
        visible: !root.activeFocus
        color: mainWindow.palette.text
        showFiat: false
    }

    Item { // edit-label
        visible: root.activeFocus
        x: 8
        y: 8
        Label {
            id: begin
            text: privValue.enteredString.substring(0, privValue.cursorPos)
        }
        Rectangle {
            id: cursor
            anchors.left: begin.right
            width: 1
            height: root.height - 16
            color: cursorVisible ? unit.palette.text : "#00000000"
            property bool cursorVisible: true
            Timer {
                id: blinkingCursor
                running: root.activeFocus
                repeat: true
                interval: 500
                onTriggered: parent.cursorVisible = !parent.cursorVisible
            }
        }
        Label {
            anchors.left: begin.right
            text: privValue.enteredString.substring(privValue.cursorPos)
        }
    }

    Label {
        id: unit
        text: Pay.unitName
        y: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        visible: root.activeFocus
    }

    Rectangle { // focus indicator
        anchors.fill: parent
        border.color: root.activeFocus ? unit.palette.highlight  : unit.palette.mid
        border.width: 1
        color: "#00000000" // transparant
    }

    Keys.onPressed: {
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
    Keys.onReleased: {
        if (event.matches(StandardKey.Paste)) {
            privValue.paste();
            event.accepted = true;
            root.valueEdited();
        }
    }
}
