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

        function encryptAndExit() {
            var account = root.account;
            if (optionsRow.selectedKey == 0)
                account.encryptPinToPay(passwordField.text);
            if (optionsRow.selectedKey == 1)
                account.encryptPinToOpen(passwordField.text);
            passwordField.text = ""
            accountOverlay.state = "showTransactions" // should close this screen
            tabbar.currentIndex = 0
        }

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
                        qsTr("Balance and history protected", "pin to open"),
                        qsTr("Requires Pin to view, sync or pay", "pin to open"),
                    ]
                }
            }
            Label {
                id: description
                anchors.left: optionsRow.left
                text: {
                    var type = optionsRow.selectedKey
                    if (type == 0)
                        return qsTr("Make \"%1\" wallet require a pin to pay").arg(portfolio.current.name);
                    return qsTr("Make \"%1\" wallet require a pin to open").arg(portfolio.current.name);
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
                Column {
                    Label {
                        text: {
                            if (root.account.needsPinToOpen)
                                return qsTr("Wallet already has pin to open applied")
                            return qsTr("Your full wallet gets encrypted, opening it will need a password. If you don't have a backup of this wallet, make one first.")
                        }
                        wrapMode: Text.WordWrap
                        width: stack.width
                    }
                    Label {
                        visible: root.account.needsPinToPay
                        text:  "This wallet already has pin to pay applied, you may upgrade it to pin to open but it will remove pin to pay. The password you provide must be the same as the one for pin to pay";
                        wrapMode: Text.WordWrap
                        width: stack.width
                    }
                }
            }

            GridLayout {
                visible: {
                    if (root.account.needsPinToPay)
                        return optionsRow.selectedKey == 1;
                    if (root.account.needsPinToOpen)
                        return false;
                    return true;
                }

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
                    id: passwordField
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    onAccepted: encryptButton.clicked()
                }
                Label {
                    visible: !portfolio.singleAccountSetup
                    text: qsTr("Wallet") + ":"
                }
                Label {
                    visible: !portfolio.singleAccountSetup
                    text: root.account.name
                }
                Item {
                    width: 1
                    height: 1
                }
                Flowee.WarningLabel {
                    Layout.fillWidth: true
                    id: warningLabel
                }
            }

            Item {
                width: parent.width
                height: closeButton.height
                Flowee.Button {
                    id: encryptButton
                    enabled: passwordField.enabled && passwordField.text.length > 3
                    text: qsTr("Encrypt")
                    anchors.right: closeButton.left
                    anchors.rightMargin: 10
                    onClicked: {
                        if (account.needsPinToPay) {
                            var ok = account.decrypt(passwordField.text);
                            if (!ok) {
                                warningLabel.text = qsTr("Invalid password to open this wallet")
                                return;
                            }
                            contentArea.encryptAndExit();
                        }
                        else {
                            passwdDialog.start(); // on click, ask for the password again
                        }
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

    Flowee.PasswdDialog {
        id: passwdDialog
        title: qsTr("Repeat password")
        text: qsTr("Please confirm the password by entering it again")

        onAccepted: {
            if (pwd !== passwordField.text) {
                // mismatching entries.
                warningLabel.text = qsTr("Typed passwords do not match")
                return;
            }
            contentArea.encryptAndExit();
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
