/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
    headerButtonVisible: true
    headerButtonText: qsTr("Next")

    onHeaderButtonClicked: {
        if (selectedKey === 0)
            var page = newBaseWalletScreen;
        if (selectedKey === 1)
            page = newHDWalletScreen;
        if (selectedKey === 2)
            page = "./ImportWalletPage.qml";
        thePile.push(page);
    }

    property int selectedKey: 1


    ColumnLayout {
        anchors.fill: parent

        Flowee.Label {
            text: qsTr("Create a New Wallet") + ":"
        }
        Flow {
            width: parent.width
            property int selectorWidth: (width - spacing * 2) / 2;
            property alias selectedKey: root.selectedKey

            Flowee.CardTypeSelector {
                id: accountTypeBasic
                key: 0
                title: qsTr("Basic")
                width: parent.selectorWidth
                onClicked: root.selectedKey = key

                features: [
                    qsTr("Private keys based", "Property of a wallet"),
                    qsTr("Difficult to backup", "Context: wallet type"),
                    qsTr("Great for brief usage", "Context: wallet type")
                ]
            }
            Flowee.CardTypeSelector {
                id: accountTypePreferred
                key: 1
                title: qsTr("HD wallet")
                width: parent.selectorWidth
                onClicked: root.selectedKey = key

                features: [
                    qsTr("Seed-phrase based", "Context: wallet type"),
                    qsTr("Easy to backup", "Context: wallet type"),
                    qsTr("Most compatible", "The most compatible wallet type")
                ]
            }
        }

        Item { width: 10; height: 15 }

        Flowee.Label {
            text: qsTr("Import Existing Wallet") + ":"
        }
        Item {
            width: parent.width
            height: accountTypeImport.height
            property alias selectedKey: root.selectedKey
            Flowee.CardTypeSelector {
                id: accountTypeImport
                key: 2
                title: qsTr("Import")
                width: Math.min(parent.width / 3 * 2, 250)
                onClicked: root.selectedKey = key
                anchors.horizontalCenter: parent.horizontalCenter

                features: [
                    qsTr("Imports seed-phrase"),
                    qsTr("Imports private key")
                ]
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
            headerButtonVisible: true
            headerButtonText: qsTr("Create")
            headerButtonEnabled: accountName.text.length > 2
            onHeaderButtonClicked: {
                var options = Pay.createNewBasicWallet(accountName.text);
                options.forceSingleAddress = singleAddress.checked;
                var accounts = portfolio.accounts;
                for (var i = 0; i < accounts.length; ++i) {
                    var a = accounts[i];
                    if (a.name === options.name) {
                        portfolio.current = a;
                        break;
                    }
                }
                thePile.pop();
                thePile.pop();
            }

            ColumnLayout {
                width: parent.width

                Flowee.Label {
                    id: title
                    text: qsTr("This creates a new empty wallet with simple multi-address capability")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Flowee.Label {
                    text: qsTr("Name") + ":";
                }
                Flowee.TextField {
                    id: accountName
                    focus: true
                    Layout.fillWidth: true
                }
                Item { width: 10; height: 10 }
                Flowee.CheckBox {
                    id: singleAddress
                    text: qsTr("Force Single Address");
                    toolTipText: qsTr("When enabled, this wallet will be limited to one address.\nThis ensures only one private key will need to be backed up")
                }
                Flowee.Label {
                    text: qsTr("Derivation") + ":"
                }
                Flowee.TextField {
                    id: derivationPath
                    Layout.fillWidth: true
                    text: "m/44'/0'/0'" // What most wallets use to import by default
                    color: Pay.checkDerivation(text) ? palette.text : "red"
                }
            }
        }
    }
    Component {
        id: newHDWalletScreen

        Page {
            headerText: qsTr("New HD-Wallet")
            headerButtonVisible: true
            headerButtonText: qsTr("Create")
            headerButtonEnabled: accountName.text.length > 2
            onHeaderButtonClicked: {
                var options = Pay.createNewWallet("m/44'/0'/0'", /* password */"", accountName.text);
                var accounts = portfolio.accounts;
                for (var i = 0; i < accounts.length; ++i) {
                    var a = accounts[i];
                    if (a.name === options.name) {
                        portfolio.current = a;
                        break;
                    }
                }
                thePile.pop();
                thePile.pop();
            }

            ColumnLayout {
                width: parent.width
                Flowee.Label {
                    id: title
                    Layout.fillWidth: true
                    text: qsTr("This creates a new empty wallet with smart creation of addresses from a single seed-phrase")
                    wrapMode: Text.WordWrap
                }
                Flowee.Label {
                    text: qsTr("Name") + ":";
                }
                Flowee.TextField {
                    id: accountName
                    Layout.fillWidth: true
                    focus: true
                }
            }
        }
    }
    }
}
