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
import QtQuick.Controls 2.11 as QQC2
import Flowee.org.pay 1.0

/*
 * Similar to TextField, this is a wrapper around BitcoinAmountLabel
 * in order to provide a background and ediing capabilities.
 */
MoneyValueField  {
    id: root
    implicitHeight: balance.height + 12
    implicitWidth: balance.width + 12

    property alias fontPixelSize: balance.fontPixelSize
    property double baselineOffset: balance.baselineOffset + balance.y

    BitcoinAmountLabel {
        id: balance
        x: 6
        y: 6
        value: root.value
        colorize: false
        visible: !root.activeFocus
        color: unit.palette.text
        showFiat: false
    }

    Label {
        id: unit
        text: Pay.unitName
        y: 6
        anchors.right: parent.right
        anchors.rightMargin: 8
        visible: root.activeFocus
    }
}
