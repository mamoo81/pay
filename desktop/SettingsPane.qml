import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0

import "./ControlColors.js" as ControlColors
import "widgets" as Flowee

Pane {
    property string title: qsTr("Settings")
    property string icon: "qrc:/settingsIcon-light.png"

    GridLayout {
        columns: 3

        Label {
            text: qsTr("Night mode") + ":"
        }
        Flowee.CheckBox {
            Layout.columnSpan: 2
            checked: Pay.useDarkSkin
            onClicked: {
                Pay.useDarkSkin = checked;
                ControlColors.applySkin(mainWindow);
            }
        }

        Label {
            text: qsTr("Unit") + ":"
        }
        ComboBox {
            width: 210
            model: {
                var answer = [];
                for (let i = 0; i < 5; ++i) {
                    answer[i] = Pay.nameOfUnit(i);
                }
                return answer;
            }
            currentIndex: Pay.unit
            onCurrentIndexChanged: {
                Pay.unit = currentIndex
            }
        }

        Rectangle {
            color: "#00000000"
            radius: 6
            border.color: mainWindow.palette.button
            border.width: 2

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

        Label {
            text: qsTr("Version") + ":"
        }
        Label {
            text: Pay.version
            Layout.columnSpan: 2
        }
        Label {
            text: qsTr("Library Version") + ":"
        }
        Label {
            text: Pay.libsVersion
            Layout.columnSpan: 2
        }
    }

    Button {
        anchors.right: parent.right
        text: qsTr("Network Status")
        onClicked: {
            netView.source = "./NetView.qml"
            netView.item.show();
        }
    }
}
