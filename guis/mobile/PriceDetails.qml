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
import Flowee.org.pay;

Item {
    id: root
    implicitHeight: mainPrice.height + changes.height + 10

    property int currentPrice: Fiat.price

    Image {
        anchors.right: parent.right
        width: 16
        height: 16
        smooth: true
        source: "qrc:/settingsIcon" + (Pay.useDarkSkin ? "-light" : "") + ".svg"
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
        text: qsTr("1 BCH is: %1", "Price of a whole bitcoin cash"). arg(Fiat.formattedPrice(100000000, root.currentPrice))
    }

    Component {
        id: historyLabel
        Item {
            property int days: 0
            height: buddy.height
            width: buddy.width + 3 + main.width

            Flowee.Label {
                id: buddy
                text: title + ":"
            }
            Flowee.Label {
                id: main
                anchors.left: buddy.right
                anchors.leftMargin: 3
                property double percentage:  {
                    var oldPrice = Fiat.historicalPrice(daysAgo);
                    return (root.currentPrice - oldPrice) / oldPrice * 100;
                }
                text: {
                    var sign = "";
                    if (percentage < 0)
                        sign = "↓";
                    else if (percentage > 0)
                        sign = "↑";
                    return sign + Math.abs(percentage.toFixed(1))+ "%"
                }

                color: {
                    if (percentage > 0)
                        return Pay.useDarkSkin ? mainWindow.floweeGreen : "#31993d";
                    return Pay.useDarkSkin ? "#ff5050" : "#c33d3d";
                }
            }
        }
    }

    Flow {
        id: changes
        anchors.top: mainPrice.bottom
        anchors.topMargin: 10
        width: parent.width
        spacing: 10
        Repeater {
            model: ListModel {
                ListElement { title: qsTr("7d", "7 days"); // 7 days
                    daysAgo: 7 }
                ListElement { title: qsTr("1m", "1 month"); // 30 days
                    daysAgo: 30 }
                ListElement { title: qsTr("3m", "3 months"); // 90 days
                    daysAgo: 90 }
            }
            delegate: historyLabel
        }
    }
}
