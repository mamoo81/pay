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
import "../ControlColors.js" as ControlColors

// This is silly to be needed, but we want to introduce a visual difference
// between enabled and disabled buttons.
QQC2.Button {
    id: button

    contentItem: Text {
        text: button.text
        font: button.font
        color: {
            if (!button.enabled)
                return Pay.useDarkSkin ? Qt.darker(palette.buttonText, 1.8) : Qt.lighter(palette.buttonText, 2);
            return palette.buttonText;
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 90
        implicitHeight: contentItem.implicitHeight + 10
        opacity: enabled ? 1 : 0.8
        border.color: Pay.useDarkSkin ? Qt.lighter(palette.button)
                                  : Qt.darker(palette.button);
        color: {
            if (button.down || button.checked)
                return Pay.useDarkSkin ? Qt.darker(palette.button)
                                       : Qt.lighter(palette.button);
            return palette.button
        }
        border.width: 1.5
        radius: 2
    }
}
