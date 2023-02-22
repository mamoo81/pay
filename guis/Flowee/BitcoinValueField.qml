/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2023 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay

MoneyValueField  {
    id: root

    property alias fontPixelSize: beginOfValue.fontPixelSize
    property alias contentWidth: row.width
    property alias color: beginOfValue.color
    baselineOffset: beginOfValue.baselineOffset

    implicitHeight: beginOfValue.implicitHeight
    implicitWidth: Math.max(row.width, 70)

    RowLayout {
        id: row
        anchors.right: parent.right
        spacing: 4
        height: parent.height
        property string amountString: {
            var fullString = Pay.amountToString(root.value);
            // for editing we should remove the trailing zeros
            // but leave the separator if that is what the user typed. So a valid string is "6."
            var userTypedString = money.enteredString;
            let length = userTypedString.length;
            for (let i = fullString.length - 1; i > length; i = i - 1) {
                let k = fullString.charAt(i);
                if (k !== '0' && k !== '.' && k !== ',') {
                    length = i + 1;
                    break;
                }
            }
            return fullString.substring(0, Math.max(1, length));
        }

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
            cursorPos: root.cursorPos
            Layout.alignment: Qt.AlignBaseline
            showCursor: root.activeFocus
        }
        LabelWithCursor {
            id: middle
            Layout.alignment: Qt.AlignBaseline
            color: beginOfValue.color
            cursorPos: root.activeFocus ? root.cursorPos :  -1
            fontPixelSize: beginOfValue.fontPixelSize / 10 * 8
            fullString: row.amountString
            textOpacity: text === "000" ? 0.3 : 1
            startPos: beginOfValue.startPos + beginOfValue.stringLength
            stringLength: 3
            showCursor: root.activeFocus
        }
        LabelWithCursor {
            id: satsLabel
            color: beginOfValue.color
            cursorPos: root.activeFocus ? root.cursorPos :  -1
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
}
