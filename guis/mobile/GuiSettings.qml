/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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
import QtQuick.Layouts
import "../Flowee" as Flowee

Page {
    headerText: qsTr("Display Settings")
    id: root

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.height
        boundsBehavior: Flickable.StopAtBounds // don't show this is a flickable if there is no space issues

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 10

            PageTitledBox {
                title: qsTr("Font sizing")
                Row {
                    width: parent.width
                    id: fontSizing
                    property double buttonWidth: width / 6
                    Repeater {
                        model: 6
                        delegate: Item {
                            width: fontSizing.buttonWidth
                            height: 30
                            property int target: index * 25 + 75

                            Rectangle {
                                width: parent.width - 5
                                x: 2.5
                                height: 3
                                color: Pay.fontScaling === target ? palette.highlight : palette.button
                            }

                            Flowee.Label {
                                font.pixelSize: 15
                                text: "" + target
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            MouseArea {
                                anchors.fill: parent
                                anchors.topMargin: -30
                                onClicked: Pay.fontScaling = target
                            }
                        }
                    }
                }
            }

            PageTitledBox {
                title: qsTr("Dark Theme")
                Flowee.RadioButton {
                    text: "Follow System"
                    checked: Pay.skinFollowsPlatform
                    onClicked: Pay.skinFollowsPlatform = true;
                }
                Flowee.RadioButton {
                    text: "Dark"
                    checked: !Pay.skinFollowsPlatform && Pay.useDarkSkin
                    onClicked: {
                        Pay.skinFollowsPlatform = false;
                        Pay.useDarkSkin = true;
                    }
                }
                Flowee.RadioButton {
                    text: "Light"
                    checked: !Pay.skinFollowsPlatform && !Pay.useDarkSkin
                    onClicked: {
                        Pay.skinFollowsPlatform = false;
                        Pay.useDarkSkin = false;
                    }
                }
            }

            PageTitledBox {
                title: qsTr("Unit")

                Flowee.ComboBox {
                    id: unitSelector
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
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#00000000"
                    radius: 6
                    border.color: palette.button
                    border.width: 0.8

                    implicitHeight: units.height + 10
                    implicitWidth: units.width + 10

                    GridLayout {
                        id: units
                        columns: 3
                        x: 5; y: 5
                        rowSpacing: 0
                        Flowee.Label {
                            text: {
                                var answer = "1";
                                for (let i = Pay.unitAllowedDecimals; i < 8; ++i) {
                                    answer += "0";
                                }
                                return answer + " " + Pay.unitName;
                            }
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.Label { text: "=" }
                        Flowee.Label { text: "1 Bitcoin Cash" }

                        Flowee.Label { text: "1 " + Pay.unitName; Layout.alignment: Qt.AlignRight; visible: Pay.isMainChain}
                        Flowee.Label { text: "="; visible: Pay.isMainChain}
                        Flowee.Label {
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
            }

            TextButton {
                Layout.fillWidth: true
                text: qsTr("Change Currency (%1)").arg(Fiat.currencyName)
                showPageIcon: true
                onClicked: thePile.push("./CurrencySelector.qml")
            }

            PageTitledBox {
                title: qsTr("Main View")
                visible: Pay.isMainChain // because we only have one option right now

                Flowee.CheckBox {
                    width: parent.width
                    text: qsTr("Show Bitcoin Cash value")
                    checked: Pay.activityShowsBch
                    onCheckedChanged: Pay.activityShowsBch = checked
                    visible: Pay.isMainChain // only mainchain has fiat value
                }
            }
        }
    }
}
