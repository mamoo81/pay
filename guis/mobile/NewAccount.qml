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
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import Flowee.org.pay

Page {
    id: root

    headerText: qsTr("New Bitcoin Cash Wallet")
    Flickable {
        anchors.fill: parent
        contentHeight: column.height + 20
        contentWidth: width

        Column {
            id: column
            width: parent.width
            y: 10
            spacing: 20

            PageTitledBox {
                title: qsTr("Create a New Wallet")
                width: parent.width
                spacing: 10

                Flowee.CardTypeSelector {
                    title: qsTr("HD wallet")
                    onClicked: thePile.push(newHDWalletScreen);
                    width: parent.width * 0.75
                    anchors.horizontalCenter: parent.horizontalCenter
                    selected: true

                    features: [
                        qsTr("Seed-phrase based", "Context: wallet type"),
                        qsTr("Easy to backup", "Context: wallet type"),
                        qsTr("Most compatible", "The most compatible wallet type")
                    ]
                    Rectangle {
                        anchors.fill: parent
                        color: "#00000000"
                        radius: 10
                        border.width: 5
                        border.color: mainWindow.floweeGreen
                    }
                }

                Flowee.CardTypeSelector {
                    title: qsTr("Basic")
                    width: parent.width * 0.75
                    onClicked: thePile.push(newBaseWalletScreen);
                    selected: true
                    anchors.horizontalCenter: parent.horizontalCenter

                    features: [
                        qsTr("Private keys based", "Property of a wallet"),
                        qsTr("Difficult to backup", "Context: wallet type"),
                        qsTr("Great for brief usage", "Context: wallet type")
                    ]
                }
            }

            PageTitledBox {
                title: qsTr("Import Existing Wallet")
                width: parent.width
                Flowee.CardTypeSelector {
                    title: qsTr("Import")
                    width: parent.width * 0.75
                    onClicked: thePile.push("./ImportWalletPage.qml");
                    anchors.horizontalCenter: parent.horizontalCenter
                    selected: true

                    features: [
                        qsTr("Imports seed-phrase"),
                        qsTr("Imports private key")
                    ]
                }
            }
        }
    }

    // ------- the next screens

    Item { // dummy item to allow components to be added to the parents GridLayout
    Component {
        id: newBaseWalletScreen

        Page {
            id: newWalletPage
            headerText: qsTr("New Wallet")

            ColumnLayout {
                width: parent.width
                spacing: 10

                Flowee.Label {
                    id: title
                    text: qsTr("This creates a new empty wallet with simple multi-address capability")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                PageTitledBox {
                    title: qsTr("Name")
                    Flowee.TextField {
                        id: accountName
                        focus: true
                        width: parent.width
                    }
                }
                Flowee.CheckBox {
                    id: singleAddress
                    text: qsTr("Force Single Address");
                    toolTipText: qsTr("When enabled, this wallet will be limited to one address.\nThis ensures only one private key will need to be backed up")
                }
                Item {
                    width: parent.width
                    height: createButton.implicitHeight
                    QQC2.Button {
                        id: createButton
                        anchors.right: parent.right
                        text: qsTr("Create")
                        onClicked: {
                            var options = Pay.createNewBasicWallet(accountName.text);
                            options.forceSingleAddress = singleAddress.checked;
                            var accounts = portfolio.accounts;
                            portfolio.current = accounts[accounts.length - 1]
                            thePile.pop();
                            thePile.pop();
                        }
                    }
                }
            }
        }
    }
    Component {
        id: newHDWalletScreen

        Page {
            headerText: qsTr("New HD-Wallet")

            ColumnLayout {
                width: parent.width
                spacing: 10
                Flowee.Label {
                    id: title
                    Layout.fillWidth: true
                    text: qsTr("This creates a new wallet that can be backed up with a seed-phrase")
                    wrapMode: Text.WordWrap
                }
                PageTitledBox {
                    title: qsTr("Name")
                    Flowee.TextField {
                        id: accountName
                        width: parent.width
                        focus: true
                    }
                }
                PageTitledBox {
                    title: qsTr("Derivation")
                    Flowee.TextField {
                        property bool derivationOk: Pay.checkDerivation(text);
                        id: derivationPath
                        width: parent.width
                        text: "m/44'/0'/0'" // What most BCH wallets are created with
                        color: derivationOk ? palette.text : "red"
                    }
                }

                Item {
                    width: parent.width
                    height: createButton.implicitHeight
                    QQC2.Button {
                        id: createButton
                        enabled: derivationPath.derivationOk
                        anchors.right: parent.right
                        text: qsTr("Create")
                        onClicked: {
                            var options = Pay.createNewWallet(derivationPath.text, /* password */"", accountName.text);
                            var accounts = portfolio.accounts;
                            portfolio.current = accounts[accounts.length - 1]
                            thePile.pop();
                            thePile.pop();
                        }
                    }
                }
            }
        }
    }
    }
}
