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

    property alias fontPixelSize: fiat.fontPixelSize
    property alias contentWidth: row.width
    property alias color: fiat.color
    baselineOffset: fiat.baselineOffset
    implicitHeight: fiat.implicitHeight
    implicitWidth: Math.max(row.width, 70)
    fiatMode: true

    RowLayout {
        id: row
        anchors.right: parent.right
        height: parent.height
        spacing: 0
        property string amountString: {
            var displayCents = Fiat.displayCents; // forcably call the method again on displayCents change
            return Fiat.priceToStringSimple(root.value)
        }

        Label {
            text: Fiat.currencySymbolPrefix
            font.pixelSize: fiat.fontPixelSize
            color: fiat.color
            visible: text !== ""
        }
        LabelWithCursor {
            id: fiat
            fullString: row.amountString
            cursorPos: root.activeFocus ? root.cursorPos :  -1
            Layout.alignment: Qt.AlignBaseline
            showCursor: root.activeFocus
        }
        Label {
            text: Fiat.currencySymbolPost
            font.pixelSize: fiat.fontPixelSize
            color: fiat.color
            visible: text !== ""
        }
    }
}
