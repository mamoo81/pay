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

Item {
    id: root
    width: row.width
    height: row.height

    property int value: 0
    property bool colorize: true

    Row {
        id: row
        property string amountString: Flowee.priceToString(value)
        height: main.height
        spacing: 6

        Text {
            id: main
            text: {
                var s = row.amountString
                return s.substring(0, s.length - 5)
            }
            color: root.colorize ? (root.value > 0 ? "green" : "#444446") : "black";
        }

        Text {
            text: {
                var s = row.amountString
                var pos = s.length - 5
                return s.substring(pos, pos + 3);
            }
            font.pointSize: sats.font.pointSize
            color: main.color
            opacity: (sats.opacity !== 1 && text == "000") ? 0.3 : 1
            anchors.baseline: main.baseline
        }
        Text {
            id: sats
            text: {
                var s = row.amountString
                return s.substring(s.length - 2);
            }
            font.pointSize: main.font.pointSize / 10 * 8
            color: main.color
            opacity: text == "00" ? 0.3 : 1
            anchors.baseline: main.baseline
        }

        Text {
            text: Flowee.unitName
            color: main.color
            anchors.baseline: main.baseline
        }
    }
}
