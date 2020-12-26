/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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
import QtQuick.Layouts 1.14

/**
 * This class displays a Bitcoin value using the current settings
 * and renders it smartly to avoid it just being a long list of digits.
 */
RowLayout {
    id: root
    property double value: 5E8
    property bool colorize: true
    property bool includeUnit: true
    property color textColor: Flowee.useDarkSkin ? "#fcfcfc" :"black"
    property var fontPtSize: 8

    height: main.height
    baselineOffset: main.baselineOffset

    // calculated
    property string amountString: "";
    Connections {
        target: Flowee
        function onUnitChanged(unit) {
            root.calcString(root.value);
        }
    }
    onValueChanged: calcString(value)
    function calcString(sats) {
        amountString = Flowee.priceToString(sats)

    }

    Label {
        id: main
        text: {
            var s = root.amountString
            var removeChars = Flowee.unitAllowedDecimals
            if (removeChars > 3)
                removeChars -= 3; // the next text field eats those
            return s.substring(0, s.length - removeChars)
        }
        color: {
            if (root.colorize) {
                var num = root.value
                if (num > 0)
                    // positive value
                    return Flowee.useDarkSkin ? "#86ffa8" : "green";
                else if (num < 0) // negative
                    return Flowee.useDarkSkin ? "#ffdede" : "#444446";
                // zero is shown without color, like below.
            }
            return root.textColor
        }
        Layout.alignment: Qt.AlignBaseline
        font.pointSize: root.fontPtSize
    }

    Label {
        text: {
            var s = root.amountString
            var pos = s.length - 5
            return s.substring(pos, pos + 3);
        }
        font.pointSize: satsLabel.font.pointSize
        color: main.color
        opacity: (satsLabel.opacity !== 1 && text == "000") ? 0.3 : 1
        Layout.alignment: Qt.AlignBaseline
        visible: Flowee.unitAllowedDecimals === 8
    }
    Label {
        id: satsLabel
        text: {
            var s = root.amountString
            return s.substring(s.length - 2);
        }
        font.pointSize: main.font.pointSize / 10 * 8
        color: main.color
        opacity: text == "00" ? 0.3 : 1
        Layout.alignment: Qt.AlignBaseline
        visible: Flowee.unitAllowedDecimals >= 2
    }

    Label {
        text: Flowee.unitName
        color: main.color
        visible: parent.includeUnit
        Layout.alignment: Qt.AlignBaseline
    }
}
