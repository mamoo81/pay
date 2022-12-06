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
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import Flowee.org.pay

FocusScope  {
    id: root
    focus: true
    activeFocusOnTab: true

    property alias value: privValue.value
    property alias fontPixelSize: beginOfValue.fontPixelSize
    property alias contentWidth: row.width
    property alias color: beginOfValue.color
    baselineOffset: beginOfValue.baselineOffset
    signal valueEdited;

    implicitHeight: beginOfValue.implicitHeight
    implicitWidth: row.width

    BitcoinValue {
        id: privValue
    }

    RowLayout {
        id: row
        spacing: 4
        height: parent.height
        property string amountString: Pay.amountToString(privValue.value)

        LabelWithCursor {
            id: beginOfValue
            fullString: row.amountString
            stringLength: {
                /*
                  Behavior differs based on the unitAllowedDecimals.
                  Basically this labelWithCursor eats up all the characters
                  not taken by the other two. But based on the unit they may not
                  be used at all (since they show smaller fonts).
                 */
                var allowedDecimals = Pay.unitAllowedDecimals;
                var removeChars = 0;
                if (allowedDecimals === 8)
                    removeChars = 5;
                else if (allowedDecimals > 0)
                    removeChars = 2 + 1;
                return fullString.length - removeChars
            }
            cursorPos: root.activeFocus ? privValue.cursorPos :  -1
            Layout.alignment: Qt.AlignBaseline
            showCursor: root.activeFocus
        }
        LabelWithCursor {
            id: middle
            Layout.alignment: Qt.AlignBaseline
            color: beginOfValue.color
            cursorPos: root.activeFocus ? privValue.cursorPos :  -1
            fontPixelSize: beginOfValue.fontPixelSize / 10 * 8
            fullString: row.amountString
            textOpacity: text === "000" ? 0.3 : 1
            startPos: beginOfValue.startPos + beginOfValue.stringLength
            stringLength: 3
            // visible: Pay.unitAllowedDecimals === 8
            showCursor: root.activeFocus
        }
        LabelWithCursor {
            id: satsLabel
            color: beginOfValue.color
            cursorPos: root.activeFocus ? privValue.cursorPos :  -1
            fontPixelSize: beginOfValue.fontPixelSize / 10 * 8
            fullString: row.amountString
            Layout.alignment: Qt.AlignBaseline
            textOpacity: text === "00" ? 0.3 : 1
            startPos: middle.startPos + middle.stringLength
            stringLength: 2
            visible: Pay.unitAllowedDecimals >= 2
            showCursor: root.activeFocus
        }

        Label {
            text: Pay.unitName
            color: beginOfValue.color
            Layout.alignment: Qt.AlignBaseline
        }
    }

    Rectangle {
        anchors.top: row.bottom
        anchors.left: row.left
        anchors.right: row.right
        height: 1.3
        color: mainWindow.palette.highlight
        visible: root.activeFocus
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.forceActiveFocus();
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
            cursor.cursorVisible = true;
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Right) {
            privValue.moveRight();
            cursor.cursorVisible = true;
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
