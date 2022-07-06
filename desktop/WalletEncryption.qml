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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "widgets" as Flowee
import "ControlColors.js" as ControlColors

FocusScope {
    id: root
    property QtObject account: portfolio.current
    focus: true

    Flickable {
        id: contentArea
        anchors.fill: parent
        anchors.margins: 10

        contentWidth: width
        contentHeight: contentAreaColumn.height + 20
        flickableDirection: Flickable.VerticalFlick
        clip: true
        ScrollBar.vertical: ScrollBar { }

        Flowee.CloseIcon {
            id: closeIcon
            anchors.right: parent.right
            onClicked: accountOverlay.state = "showTransactions"
        }
        Column {
            id: contentAreaColumn
            width: contentArea.width - 40
            y: 10
            x: 20
            spacing: 20
            Label {
                text: qsTr("Protect your wallet with a password")
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

                property int selectorWidth: (width - spacing * 2) / 2;
                property int selectedKey: 0

                CardTypeSelector {
                    id: pinToPay
                    key: 0
                    title: qsTr("Pin to Pay")
                    width: parent.selectorWidth

                    features: [
                        qsTr("Protect your funds", "pin to pay"),
                        qsTr("Fully open, except for sending funds", "pin to pay"),
                        qsTr("Keeps in sync", "pin to pay"),
                    ]
                }
                CardTypeSelector {
                    id: pinToOpen
                    key: 1
                    title: qsTr("Pin to Open")
                    width: parent.selectorWidth

                    features: [
                        qsTr("Protect your entire wallet", "pin to open"),
                        qsTr("Balance or history protected", "pin to open"),
                        qsTr("Requires PIN to view, sync or pay", "pin to open"),
                    ]
                }
            }
            Label {
                id: description
                anchors.left: optionsRow.left
                text: {
                    var type = optionsRow.selectedKey
                    if (type == 0)
                        return qsTr("Make \"%1\" require a pin to pay").arg(portfolio.current.name);
                    return qsTr("Make \"%1\" require a pin to open").arg(portfolio.current.name);
                }
                font.pointSize: 14
            }
            StackLayout {
                id: stack
                currentIndex: optionsRow.selectedKey
                width: parent.width

                Label {
                    text: {
                        if (root.account.needsPinToPay)
                            return qsTr("Wallet already has pin to pay applied")
                        if (root.account.needsPinToOpen)
                            return qsTr("Wallet already has pin to open applied")
                        return qsTr("Your wallet will get partially encrypted and payments will become impossible without a password. If you don't have a backup of this wallet, make one first.")
                    }
                    wrapMode: Text.WordWrap
                    width: stack.width
                }
                Label {
                    text: {
                        if (root.account.needsPinToOpen)
                            return qsTr("Wallet already has pin to open applied")
                        return qsTr("Your full wallet gets encrypted, opening it will need a password. If you don't have a backup of this wallet, make one first.")
                    }
                    wrapMode: Text.WordWrap
                    width: stack.width
                }
            }

            GridLayout {
                enabled: optionsRow.selectedKey == 0 && !root.account.needsPinToPay
                         || optionsRow.selectedKey == 1 && !root.account.needsPinToOpen
                width: parent.width
                columns: 2
                Label {
                    text: qsTr("Password") + ":"

                    onEnabledChanged: updateColors();
                    Connections {
                        target: Pay
                        function onUseDarkSkinChanged() { updateColors(); }
                    }
                    function updateColors() {
                        ControlColors.applySkin(this);
                        if (enabled)
                            color = palette.text
                        else
                            color = Pay.useDarkSkin ? Qt.darker(palette.text) : Qt.lighter(palette.text, 2)
                    }
                }
                Flowee.TextField {
                    id: pwd
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                }
                Label {
                    text: qsTr("Wallet") + ":"
                }
                Label {
                    text: root.account.name
                }
            }

            Item {
                width: parent.width
                height: closeButton.height
                Flowee.Button {
                    id: encryptButton
                    enabled: pwd.enabled && pwd.text.length > 3
                    text: qsTr("Encrypt")
                    anchors.right: closeButton.left
                    anchors.rightMargin: 10
                    onClicked:  {
                        // TODO unlock a wallet first with the given password if needed.

                        if (optionsRow.selectedKey == 0)
                            root.account.encryptPinToPay(pwd.text);
                        if (optionsRow.selectedKey == 1)
                            root.account.encryptPinToOpen(pwd.text);
                        pwd.text = ""
                        accountOverlay.state = "showTransactions" // aka close dialog
                    }
                }
                Flowee.Button {
                    id: closeButton
                    text: qsTr("Close")
                    anchors.right: parent.right
                    onClicked: accountOverlay.state = "showTransactions"
                }
            }
        }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Escape) {
            accountOverlay.state = "showTransactions";
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Left && optionsRow.activeFocus) {
            optionsRow.selectedKey = Math.max(0, optionsRow.selectedKey - 1)
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Right && optionsRow.activeFocus) {
            optionsRow.selectedKey = Math.min(2, optionsRow.selectedKey + 1)
            event.accepted = true;
        }
    }
}
