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
import "../ControlColors.js" as ControlColors

// This is a silly hack to introduce a visual difference
// between enabled and disabled buttons.
Button {
    id: button
    onEnabledChanged: updateColors();
    Connections {
        target: Pay
        function onUseDarkSkinChanged() { updateColors(); }
    }

    function updateColors() {
        ControlColors.applySkin(this);
        if (!enabled) {
            palette.buttonText = Pay.useDarkSkin
                ? Qt.darker(palette.buttonText)
                : Qt.lighter(palette.buttonText, 2)
            palette.button = Qt.darker(palette.button, 1.2)
        }
    }
}
