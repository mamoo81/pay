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
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11

import "../ControlColors.js" as ControlColors
import "../Flowee" as Flowee

Pane {
    property string title: qsTr("Settings")
    property string icon: "qrc:/settingsIcon-light.png"

    GridLayout {
        columns: 3

        Label {
            text: qsTr("Unit") + ":"
            Layout.alignment: Qt.AlignRight
        }

        Flowee.ComboBox {
            id: unitSelector
            width: 210
            model: {
                var answer = [];
                for (let i = 0; i < 5; ++i) {
                    answer[i] = Pay.nameOfUnit(i);
                }
                return answer;
            }
            currentIndex: Pay.unit
            onCurrentIndexChanged: Pay.unit = currentIndex
        }

        Rectangle {
            color: "#00000000"
            radius: 6
            border.color: mainWindow.palette.button
            border.width: 0.8

            implicitHeight: units.height + 10
            implicitWidth: units.width + 10

            GridLayout {
                id: units
                columns: 3
                x: 5; y: 5
                rowSpacing: 0
                Label {
                    text: {
                        var answer = "1";
                        for (let i = Pay.unitAllowedDecimals; i < 8; ++i) {
                            answer += "0";
                        }
                        return answer + " " + Pay.unitName;
                    }
                    Layout.alignment: Qt.AlignRight
                }
                Label { text: "=" }
                Label { text: "1 Bitcoin Cash" }

                Label { text: "1 " + Pay.unitName; Layout.alignment: Qt.AlignRight; visible: Pay.isMainChain}
                Label { text: "="; visible: Pay.isMainChain}
                Label {
                    text: {
                        var amount = 1;
                        for (let i = 0; i < Pay.unitAllowedDecimals; ++i) {
                            amount = amount * 10;
                        }
                        return Fiat.formattedPrice(amount, Fiat.price);
                    }
                    visible: Pay.isMainChain
                }
            }
        }

        Flowee.CheckBox {
            id: showBlockNotificationsChooser
            Layout.alignment: Qt.AlignRight
            checked: !Pay.newBlockMuted
            onCheckedChanged: Pay.newBlockMuted = !checked;
        }
        Flowee.CheckBoxLabel {
            Layout.columnSpan: 2
            buddy: showBlockNotificationsChooser
            text: qsTr("Show Block Notifications")
            toolTipText: qsTr("When a new block is mined, Flowee Pay shows a desktop notification")
        }
        Flowee.CheckBox {
            id: darkSkinChooser
            Layout.alignment: Qt.AlignRight
            checked: Pay.useDarkSkin
            onCheckedChanged: {
                Pay.useDarkSkin = checked
                ControlColors.applySkin(mainWindow);
            }
        }
        Flowee.CheckBoxLabel {
            Layout.columnSpan: 2
            buddy: darkSkinChooser
            text: qsTr("Night Mode")
        }

        Label {
            text: qsTr("Version") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: Pay.version
            Layout.columnSpan: 2
        }
        Label {
            text: qsTr("Library Version") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: Pay.libsVersion
            Layout.columnSpan: 2
        }
        Label {
            text: qsTr("Synchronization") + ":"
        }

        Button {
            Layout.columnSpan: 2
            text: qsTr("Network Status")
            onClicked: {
                netView.source = "./NetView.qml"
                netView.item.show();
            }
        }
    }
}
