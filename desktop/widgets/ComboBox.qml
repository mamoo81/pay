/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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

QQC2.ComboBox {
    id: root

    // The default theme settings have a bug we fix here.
    // The default QQC2 combobox has in its delegate a color setting that in dark mode
    // means we have dark text on black background.
    // Qt made the mistake of mixing palette.highlightedText with palette.dark, which is
    // guarenteed to cause issues in custom palette setups, like our darkmode.
    delegate: QQC2.ItemDelegate {
        id: basicButton
        width: root.width
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: modelData
            color: basicButton.highlighted ? root.palette.highlightedText : root.palette.buttonText
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: basicButton.highlighted ? root.palette.highlight : root.palette.button
        }
    }

    // Personal preference, the single arrow down makes more sense to me.
    // We've had that for decades.
    indicator: Canvas {
        id: canvas
        x: root.width - width - root.rightPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        width: 12
        height: 8
        contextType: "2d"
        onPaint: {
            context.reset();
            context.moveTo(0, 0);
            context.lineTo(width, 0);
            context.lineTo(width / 2, height);
            context.closePath();
            context.fillStyle = root.palette.light
            context.fill();
        }
    }
}
