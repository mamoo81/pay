/* * This file is part of the Flowee project
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

FocusScope {
    id: root
    anchors.fill: parent
    focus: true

    Rectangle {
        color: "black"
        anchors.fill: parent
        opacity: 0.4
        MouseArea {
            anchors.fill: parent
            onClicked: root.visible = false
        }
    }
    Rectangle {
        color: mainWindow.palette.window
        anchors.fill: contentArea
        MouseArea { anchors.fill: parent }
    }

    Flickable {
        id: contentArea
        anchors.fill: parent
        anchors.margins: 50

        contentWidth: width
        contentHeight: contentAreaColumn.height + 20
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: contentAreaColumn
            width: contentArea.width
            y: 10
            Label {
                text: qsTr("New Bitcoin Cash Wallet")
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 14
            }
            Flow {
                id: optionsRow
                anchors.horizontalCenter: parent.horizontalCenter
                activeFocusOnTab: true
                focus: true
                height: 320
                spacing: 20
                width: Math.min(contentArea.width - 30, 900) // smaller is OK, wider not

                property int selectorWidth: (width - spacing * 2) / 3;

                property int selectedAccountType: 1
                AccountTypeSelector {
                    id: accountTypeBasic
                    accountType: 0
                    title: qsTr("Basic")
                    width: parent.selectorWidth

                    features: [
                        qsTr("Private keys", "Property of a wallet"),
                        qsTr("Backups difficult", "Context: wallet type"),
                        qsTr("Great for brief usage", "Context: wallet type")
                    ]
                }
                AccountTypeSelector {
                    id: accountTypePreferred
                    accountType: 1
                    title: qsTr("HD wallet")
                    width: parent.selectorWidth

                    features: [
                        qsTr("Seed phrase based", "Context: wallet type"),
                        qsTr("Easy to backup", "Context: wallet type"),
                        qsTr("Hierarchically Deterministic", "BIP32 term"),
                        qsTr("Most compatible type", "The most compatible wallet type")
                    ]
                }
                AccountTypeSelector {
                    id: accountTypeImport
                    accountType: 2
                    title: qsTr("Import")
                    width: parent.selectorWidth

                    features: [
                        qsTr("Imports seeds"),
                        qsTr("Imports paper wallets")
                    ]
                }
            }

            Item { width: 1; height: 30 } // spacer

            Label {
                id: description
                anchors.left: optionsRow.left
                text: {
                    var type = optionsRow.selectedAccountType
                    if (type == 0)
                        return qsTr("Basic wallet with private keys")
                    if (type == 1)
                        return qsTr("Single seed-phrase wallet")
                    return qsTr("Import existing wallet")
                }
                font.pointSize: 14
            }
            Item { width: 1; height: 20 } // spacer
            StackLayout {
                id: stack
                currentIndex: optionsRow.selectedAccountType
                anchors.left: optionsRow.left
                anchors.right: optionsRow.right
                height: 1
                onCurrentIndexChanged: fixHeight()
                Component.onCompleted: fixHeight()
                function fixHeight() {
                    // TODO why is this needed?
                    var cur = visibleChildren[0]
                    height = cur.implicitHeight
                }

                NewAccountCreateBasicAccount {
                }
                GridLayout {
                    id: createHDAccount
                    columns: 1
                    Label {
                        text: qsTr("This creates a new empty wallet with smart creation of addresses from a single seed")
                        Layout.columnSpan: 1
                    }
                }
                NewAccountImportAccount {
                }
            }
        }
    }

    Component.onCompleted: forceActiveFocus() // we assume this component is used in a Loader
    Keys.onPressed: {
        if (event.key === Qt.Key_Escape) {
            root.visible = false;
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Left && optionsRow.activeFocus) {
            optionsRow.selectedAccountType = Math.max(0, optionsRow.selectedAccountType - 1)
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Right && optionsRow.activeFocus) {
            optionsRow.selectedAccountType = Math.min(2, optionsRow.selectedAccountType + 1)
            event.accepted = true;
        }
    }
}
