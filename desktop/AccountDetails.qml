/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay 1.0

Item {
    id: root
    property QtObject account: portfolio.current

    Label {
        id: walletDetailsLabel
        text: qsTr("Advanced Wallet Details")
        font.pointSize: 18
        color: "white"
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Item {
        id: closeIcon
        width: 30
        height: 30
        anchors.right: parent.right
        anchors.margins: 6
        anchors.bottom: walletDetailsLabel.bottom
        MouseArea {
            anchors.fill: parent
            onClicked: accountDetails.state = "showTransactions"
        }

        Rectangle {
            color: "#bbbbbb"
            width: 30
            height: 3
            radius: 3
            rotation: 45
            anchors.centerIn: parent
        }
        Rectangle {
            color: "#bbbbbb"
            width: 30
            height: 3
            radius: 3
            rotation: -45
            anchors.centerIn: parent
        }
    }

    Flickable {
        anchors.top: walletDetailsLabel.bottom
        anchors.topMargin: 10
        anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: gridlayout.height + keysTable.height + 20
        clip: true

        GridLayout {
            id: gridlayout
            columns: 3
            columnSpacing: 6
            rowSpacing: 10
            width: parent.width

            RowLayout {
                Layout.columnSpan: 3
                Label {
                    text: qsTr("Name") + ":"
                }
                FloweeTextField {
                    id: accountNameEdit
                    text: root.account.name
                    onTextEdited: root.account.name = text
                    Layout.fillWidth: true
                }
            }
            Label {
                text: qsTr("Sync Status") + ":"
            }
            Label {
                text: syncIndicator.accountBlockHeight + " / " + syncIndicator.globalPos
            }

/* TODO, features to add;
            Label {
                text: qsTr("Security:")
                Layout.columnSpan: 3
                font.italic: true
            }
    
            FloweeCheckBox {
                id: schnorr
                checked: true
            }
            Label {
                text: qsTr("Use Schnorr signatures");
            }
            Item { // spacer
                width: 1
                height: 1
                Layout.fillWidth: true
                Layout.rowSpan: 5
            }
            FloweeCheckBox {
                id: pinToTray
                checked: false
            }
            Label {
                text: qsTr("Pin to Pay");
            }
            FloweeCheckBox {
                id: pinToOpen
                checked: false
            }
            Label {
                text: qsTr("Pin to Open");
            }
            FloweeCheckBox {
                id: syncOnStart
                checked: false
            }
            Label {
                text: qsTr("Sync on Start");
            }
            FloweeCheckBox {
                id: useIndexServer
                checked: false
            }
            RowLayout {
                Label {
                    text: qsTr("Use Indexing Server");
                }
                Label { text: "?" }
            }
*/
    
            RowLayout {
                Layout.columnSpan: 3
                Label {
                    id: walletType
                    text: {
                        if (root.account.isSingleAddressAccount)
                            return qsTr("This account is a single-address wallet.")
    
                         // ok we only have one more type so far, so this is rather simple...
                        return qsTr("This account is a simple multiple-address wallet.")
                    }
                }
                /* TODO make able to convert to another type of wallet
                Image {
                    id: editWalletType
                    source: Flowee.useDarkSkin ? "qrc:///images/gear-white.svg" : "qrc:///images/gear.svg"
                    sourceSize.width: 24
                    sourceSize.height: 24
                }
                */
            }
        }

        Rectangle {
            id: keysTable
            border.color: "black"
            border.width: 3
            color: "#00000000"
            radius: 3
            anchors.top: gridlayout.bottom
            anchors.topMargin: 20
            width: parent.width
            height: rows.height + 10

            Column {
                id: rows
                width: parent.width - 6
                spacing: 10
                x: 3
                y: 3

                Rectangle {
                    id: header
                    color: Flowee.useDarkSkin ? "#393939" : "#bfbfbf"
                    width: parent.width
                    height: headerLabel.height + 18
                    Label {
                        id: headerLabel
                        text: qsTr("Keys in this wallet")
                        anchors.centerIn: parent
                    }
                }
                Repeater {
                    model: root.account.walletSecrets
                    delegate: Rectangle {
                        color: (index % 2) == 0 ? mainWindow.palette.base : mainWindow.palette.alternateBase
                        width: root.width
                        height: id.height

                        Label {
                            id: id
                            text: modelData.id
                            x: 10
                        }
                        LabelWithClipboard {
                            x: 40
                            text: modelData.address
                        }

                        Label {
                            text: qsTr("unused")
                            anchors.right: button.left
                            anchors.rightMargin: 10
                            visible: !bitcoinAmountLabel.visible
                        }
                        BitcoinAmountLabel {
                            id: bitcoinAmountLabel
                            value: modelData.saldo
                            colorize: false
                            visible: modelData.used || modelData.saldo > 0 // we either signed or we have UTXOs
                            anchors.right: button.left
                            anchors.rightMargin: 10
                        }
                        ConfigItem {
                            id: button
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            wide: true
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -3
                                onClicked: contextMenu.popup()

                                Menu {
                                    id: contextMenu
                                    MenuItem {
                                        text: qsTr("Copy Address")
                                        onTriggered: Flowee.copyToClipboard(modelData.address)
                                    }
                                    MenuItem {
                                        text: qsTr("Copy Private Key")
                                        onTriggered: Flowee.copyToClipboard(modelData.fetchPrivateKey())
                                    }
                                    /*
                                    MenuItem {
                                        text: qsTr("Move to New Wallet")
                                        onTriggered: ;
                                    } */
                                }
                            }
                        }
                    }
                }
            }

        }
    }
}
