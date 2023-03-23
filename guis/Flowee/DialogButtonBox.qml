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
import QtQuick
import QtQuick.Controls as QQC2

/*
 * This is a buttonbox that swaps button order based on platform defaults.
 *
 * This file fixes the DialogButtonBox having really bad defaults.
 * Specifically, it doesn't align the buttons right, but uses the
 * full width (making the buttons too big).
 * It doesn't have any spacing between the buttons.
 * Also it removes spacing above and below that should not be there.
 */
QQC2.DialogButtonBox {
    id: root
    spacing: 0
    padding: 0

    contentItem: ListView {
        model: root.contentModel
        spacing: 10
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }

    function setEnabled(buttonType, on) {
        let but = standardButton(buttonType)
        if (but !== null)
            but.enabled = on
    }

    alignment: Qt.AlignRight
    delegate: Button {
            width: Math.min(
                implicitWidth,
                // Divide availableWidth (width - leftPadding - rightPadding) by the number of buttons,
                // then subtract the spacing between each button.
                (root.availableWidth / root.count) - (root.spacing * (root.count-1)
            ))
        }

}

