/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay 1.0

/*
 * A simple label that shows the amount of time a pair of block-heights are apart.
 */
Label {
    property int accountBlockHeight: 0
    property int globalPos: Flowee.chainHeight

    text: {
        var diff = globalPos - accountBlockHeight
        if (diff < 0) // we are ahead???
            return "--"
        var days = diff / 144
        var weeks = diff / 1008
        if (days > 10)
            return qsTr("%1 weeks behind", "", Math.ceil(weeks)).arg(Math.ceil(weeks));
        var hours = diff / 6
        if (hours > 48)
            return qsTr("%1 days behind", "", Math.ceil(days)).arg(Math.ceil(days));
        if (diff === 0)
            return qsTr("Up to date")
        if (diff < 3)
            return qsTr("Updating");
        return qsTr("%1 hours behind", "", Math.ceil(hours)).arg(Math.ceil(hours));
    }
    font.italic: true
}
