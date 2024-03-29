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
import QtQuick.Controls as QQC2
import QtQuick.Layouts

/**
 * This class displays a Bitcoin value using the current settings
 * and renders it smartly to avoid it just being a long list of digits.
 */
QQC2.Control {
    id: root
    // Notice that the default is not a valid value so we can be certain that
    // we get the onValueChanged() callback.
    property double value: 0.123
    /// When colorize is true can turn the label
    /// green or red based on the value. Making this property
    /// hold a different number than 'value' will allow things like
    /// a red colored label without a minus.
    property double colorizeValue: value
    property bool colorize: true
    property bool includeUnit: true
    property bool showFiat: true
    /**
     * If this is set to something other than zero we assume its a unix
     * date and the fiat price is calculated based on the historical price
     * This is only used if you fill it with a date object.
     */
    property var fiatTimestamp: undefined
    property color color: palette.windowText
    property alias fontPixelSize: main.font.pixelSize

    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth

    height: main.height
    width: implicitWidth
    baselineOffset: main.baselineOffset

    onValueChanged: row.calcString(value)

    RowLayout {
        id: row
        height: parent.height

        // calculated
        property string amountString: "";
        Connections {
            target: Pay
            function onUnitChanged(unit) {
                row.calcString(root.value);
            }
        }
        function calcString(sats) {
            amountString = Pay.amountToString(sats)
        }

        Label {
            id: main
            text: {
                var s = row.amountString
                var removeChars = Pay.unitAllowedDecimals
                if (removeChars > 3)
                    removeChars -= 3; // the next text field eats those
                return s.substring(0, s.length - removeChars)
            }
            color: {
                if (root.colorize) {
                    let num = root.colorizeValue
                    if (num > 0)
                        // positive value
                        return Pay.useDarkSkin ? "#86ffa8" : "green";
                    else if (num < 0) // negative
                        return Pay.useDarkSkin ? "#ffc0c0" : "#5f1717";
                    // zero is shown in normal color
                }
                return root.color
            }
            Layout.alignment: Qt.AlignBaseline
        }

        Label {
            text: {
                var s = row.amountString
                var pos = s.length - 5
                return s.substring(pos, pos + 3);
            }
            font.pixelSize: satsLabel.font.pixelSize
            color: main.color
            opacity: (satsLabel.opacity !== 1 && text === "000") ? 0.3 : 1
            Layout.alignment: Qt.AlignBaseline
            visible: Pay.unitAllowedDecimals === 8
        }
        Label {
            id: satsLabel
            text: {
                var s = row.amountString
                return s.substring(s.length - 2);
            }
            font.pixelSize: main.font.pixelSize * 0.8
            color: main.color
            opacity: text === "00" ? 0.3 : 1
            Layout.alignment: Qt.AlignBaseline
            visible: Pay.unitAllowedDecimals >= 2
        }

        Label {
            text: Pay.unitName
            font.pixelSize: main.font.pixelSize
            color: main.color
            visible: root.includeUnit
            Layout.alignment: Qt.AlignBaseline
        }

        Label {
            visible: root.showFiat
            Layout.alignment: Qt.AlignBaseline
            text: {
                var fiatPrice;
                if (root.fiatTimestamp == undefined)
                    fiatPrice = Fiat.price; // todays price
                else
                    fiatPrice = Fiat.historicalPrice(root.fiatTimestamp);
                return Fiat.formattedPrice(root.value, fiatPrice)
            }
        }
    }
}
