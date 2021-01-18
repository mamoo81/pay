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
import QtQuick 2.14
import QtQuick.Controls 2.14
import Flowee.org.pay 1.0

/*
 * Similar to TextField, this is a wrapper around BitcoinAmountLabel
 * in order to provide a background and ediing capabilities.
 */
FocusScope {
    id: root
    height: balance.height + 16
    width: balance.width + 16
    activeFocusOnTab: true

    property alias value: privValue.value
    property alias valueObject: privValue
    property alias fontPtSize: balance.fontPtSize
    property bool enabled: true

    onEnabledChanged: {
        if (!enabled)
            root.focus = false
    }

    function reset() {
        privValue.enteredString = "0";
        privValue.value = 0;
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
        textColor: mainWindow.palette.text
    }

    Label {
        text: privValue.enteredString
        visible: root.activeFocus
        x: 8
        y: 8
    }
    Label {
        id: unit
        text: Flowee.unitName
        y: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        visible: root.activeFocus
    }

    Rectangle { // focus indicator
        anchors.fill: parent
        border.color: root.activeFocus ? unit.palette.highlight  : unit.palette.mid
        border.width: 2
        color: "#00000000" // transparant
    }

    Keys.onPressed: {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            privValue.addNumber(event.key);
            event.accepted = true
        }
        else if (event.key === 44 || event.key === 46) {
            privValue.addSeparator();
            event.accepted = true
        }
        else if (event.key === Qt.Key_Backspace) {
            privValue.backspacePressed();
            event.accepted = true
        }
    }
    Shortcut {
        sequence: StandardKey.Paste
        onActivated: privValue.paste()
    }
}
