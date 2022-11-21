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

Page {
    id: root

    headerText: qsTr("New Bitcoin Cash Wallet")
    headerButtonVisible: true
    headerButtonText: qsTr("Next")

    onHeaderButtonClicked: {
        if (selector.selectedKey === 0)
            var page = newBaseWalletScreen;
        if (selector.selectedKey === 1)
            page = newHDWalletScreen;
        if (selector.selectedKey === 2)
            page = importWalletScreen;
        thePile.push(page);
    }

    ColumnLayout {
        id: selector
        width: parent.width
        property int selectedKey: 1

        Flowee.Label {
            text: qsTr("Create a New Wallet") + ":"
        }
        Flow {
            width: parent.width
            property int selectorWidth: (width - spacing * 2) / 2;
            property alias selectedKey: selector.selectedKey

            Flowee.CardTypeSelector {
                id: accountTypeBasic
                key: 0
                title: qsTr("Basic")
                width: parent.selectorWidth
                onClicked: selector.selectedKey = key

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
                onClicked: selector.selectedKey = key

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
            property alias selectedKey: selector.selectedKey
            Flowee.CardTypeSelector {
                id: accountTypeImport
                key: 2
                title: qsTr("Import")
                width: Math.min(parent.width / 3 * 2, 250)
                onClicked: selector.selectedKey = key
                anchors.horizontalCenter: parent.horizontalCenter

                features: [
                    qsTr("Imports seed-phrase"),
                    qsTr("Imports private key")
                ]
            }
        }

        // ------- the next screens

        Component {
            id: newBaseWalletScreen

            Page {
                headerText: qsTr("New Wallet")
                Rectangle {
                    width: parent.width
                    implicitHeight: 100
                    color: "red"
                }
            }
        }
        Component {
            id: newHDWalletScreen

            Page {
                headerText: qsTr("New HD-Wallet")
                Rectangle {
                    width: parent.width
                    implicitHeight: 100
                    color: "blue"

                }
            }
        }
        Component {
            id: importWalletScreen

            Page {
                headerText: qsTr("Import Wallet")
                Rectangle {
                    width: parent.width
                    implicitHeight: 100
                    color: "yellow"

                }
            }
        }
    }
}
