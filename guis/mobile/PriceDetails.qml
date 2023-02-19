/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee

Item {
    id: root
    implicitHeight: mainPrice.height + changes.height + 10

    property int currentPrice: Fiat.price
    function changeComparedTo(daysAgo) {
        var oldPrice = Fiat.historicalPrice(daysAgo);
        var percentage = oldPrice / root.currentPrice * 100 - 100;
        var sign = "";
        if (percentage < 0)
            sign = "↓";
        else if (percentage > 0)
            sign = "↑";
        return sign + Math.abs(percentage.toFixed(2))+ "%"
    }

    Rectangle {
        color: "red"
        anchors.right: parent.right
        opacity: 0.3
        width: 20
        height: 20
        MouseArea {
            anchors.fill: parent
            onClicked: {
                thePile.push("./CurrencySelector.qml")
                popupOverlay.close();
            }
        }
    }

    Flowee.Label {
        id: mainPrice
        text: qsTr("1 BCH is: %1").arg(Fiat.formattedPrice(100000000, root.currentPrice))
    }

    Flow {
        id: changes
        anchors.top: mainPrice.bottom
        anchors.topMargin: 10
        width: parent.width
        spacing: 4
        Flowee.Label {
            text: qsTr("7d") + ":"; // 7 days
        }
        Flowee.Label {
            text: root.changeComparedTo(7);
            color: text.substring(0, 1) === '↓' ? "red" : "green"
        }
        Flowee.Label {
            text: "  " + qsTr("1m") + ":"; // 30 days
        }
        Flowee.Label {
            text: root.changeComparedTo(30);
            color: text.substring(0, 1) === '↓' ? "red" : "green"
        }
        Flowee.Label {
            text: "  " + qsTr("3m") + ":"; // 90 days
        }
        Flowee.Label {
            text: root.changeComparedTo(90);
            color: text.substring(0, 1) === '↓' ? "red" : "green"
        }
    }
}
