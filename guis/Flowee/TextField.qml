/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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

/*
 * Sane defaults.
 */
QQC2.TextField {
    id: root
    selectByMouse: true
    // In Qt6.3 this is just always black, so adjust to color
    placeholderTextColor: Pay.useDarkSkin ? "#cecece" : "#3e3e3e"

    background: Rectangle {
            implicitHeight: root.contentHeight + 2
            implicitWidth: 140
            color: {
                if (root.enabled)
                    return root.palette.base;
                return "#00000000";
            }
            border.color: {
                if (root.enabled)
                    return root.activeFocus ? root.palette.highlight : root.palette.button
                return "transparant";
            }
            border.width: 0.8
    }
}
