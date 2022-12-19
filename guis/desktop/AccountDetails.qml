/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
import QtQuick.Controls
import QtQuick.Layouts
import "../Flowee" as Flowee

Item {
    id: root
    property QtObject account: portfolio.current
    focus: true

    Connections {
        target: portfolio
        function onCurrentChanged() {
            // The model in this case follows the UI, so we need to have this not very
            // declarative code.
            // When a new model is offered, copy the UI settings into it.
            if (root.account.isDecrypted) {
                root.account.secrets.showChangeChain = changeAddresses.checked
                root.account.secrets.showUsedAddresses = usedAddresses.checked
            }
        }
    }

    Component.onCompleted: {
        if (root.account.isDecrypted) {
            root.account.secrets.showChangeChain = changeAddresses.checked
            root.account.secrets.showUsedAddresses = usedAddresses.checked
        }
    }

    Label {
        id: walletDetailsLabel
        text: qsTr("Wallet Details")
        font.pointSize: 18
        color: "white"
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Flowee.CloseIcon {
        id: closeIcon
        anchors.bottom: walletDetailsLabel.bottom
        anchors.margins: 6
        anchors.right: parent.right
        onClicked: accountOverlay.state = "showTransactions"
    }

    GridLayout {
        id: basicProperties
        anchors.top: walletDetailsLabel.bottom
        anchors.topMargin: 20
        x: 20
        width: parent.width - 30
        columns: 2
        rowSpacing: 10

        Label {
            text: qsTr("Name") + ":"
        }
        Flowee.TextField {
            id: accountNameEdit
            text: root.account.name
            onTextEdited: root.account.name = text
            Layout.fillWidth: true
            focus: true
        }
    }


    Flickable {
        id: scrollablePage
        anchors.top: basicProperties.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        clip: true

        contentHeight: detailsPane.height + 10 + addressesList.height + 10 + hdDetails.height + 10

        GridLayout {
            id: detailsPane
            width: parent.width - 20
            x: 10
            columns: 2

            Label {
                Layout.columnSpan: 2
                text: {
                    var height = root.account.lastBlockSynched
                    if (height < 1)
                        return ""
                    var time = Pay.formatDateTime(root.account.lastBlockSynchedTime);
                    if (time !== "")
                        time = "  (" + time + ")";
                    return qsTr("Sync Status") + ": " + height + " / " + Pay.chainHeight + time;
                }
            }
            Flowee.AccountTypeLabel {
                Layout.columnSpan: 2
                account: root.account
            }
            Label {
                id: xpubLabel
                // at the moment I don't see a point if making this translatable. Let me know if that should change!
                text: "xpub" + ":"
                visible: root.account.isHDWallet
            }
            Flowee.LabelWithClipboard {
                Layout.fillWidth: true
                visible: xpubLabel.visible
                text: root.account.xpub
            }

            Label {
                id: encLabel
                text: qsTr("Encryption") + ":"
                visible: encStatus.visible
            }
            WalletEncryptionStatus {
                id: encStatus
                Layout.fillWidth: true
                account: root.account
            }

            Label {
                id: pwdLabel
                text: qsTr("Password") + ":"
                visible: encStatus.visible
            }
            Flowee.TextField {
                id: passwordField
                onAccepted: decryptButton.clicked()
                enabled: !root.account.isDecrypted
                Layout.fillWidth: true
                echoMode: TextInput.Password
                visible: pwdLabel.visible
            }
            Item {
                visible: pwdLabel.visible
                Layout.fillWidth: true
                Layout.columnSpan: 2
                implicitHeight: Math.max(decryptWarning.implicitHeight, decryptButton.implicitHeight)

                Flowee.WarningLabel {
                    id: decryptWarning
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.right: decryptButton.left
                    anchors.rightMargin: 10
                    anchors.baseline: decryptButton.baseline
                }

                Flowee.Button {
                    id: decryptButton
                    anchors.right: parent.right
                    text: qsTr("Open")
                    enabled: passwordField.text.length > 3
                    onClicked: {
                        var rc = root.account.decrypt(passwordField.text);
                        if (rc) {
                            // decrypt went Ok
                            decryptWarning.text = ""
                            passwordField.text = ""
                        }
                        else {
                            decryptWarning.text = qsTr("Invalid PIN")
                            passwordField.forceActiveFocus();
                        }
                    }
                }
            }
        }

        Flowee.GroupBox {
            id: addressesList
            width: parent.width
            anchors.top: detailsPane.bottom
            anchors.topMargin: 10
            title: qsTr("Address List")
            collapsed: !root.account.isSingleAddressAccount
            visible: root.account.isDecrypted || !root.account.needsPinToOpen

            Flowee.CheckBox {
                id: changeAddresses
                text: qsTr("Change Addresses")
                visible: root.account.isHDWallet
                onClicked: root.account.secrets.showChangeChain = checked
                tooltipText: qsTr("Switches between addresses others can pay you on, and addresses the wallet uses to send change back to yourself.")
            }
            Flowee.CheckBox {
                id: usedAddresses
                text: qsTr("Used Addresses");
                visible: !root.account.isSingleAddressAccount
                onClicked: root.account.secrets.showUsedAddresses = checked
                tooltipText: qsTr("Switches between unused and used Bitcoin addresses")
            }
            Flowee.WalletSecretsView {
                id: historyView
                Layout.fillWidth: true
                ScrollBar.vertical: Flowee.ScrollThumb {
                    id: thumb
                    minimumSize: 20 / activityView.height
                    visible: size < 0.9
                }
                account: root.account
            }
        }
        Flowee.GroupBox {
            id: hdDetails
            width: parent.width
            anchors.top: addressesList.bottom
            anchors.topMargin: 20
            title: qsTr("Backup details")
            visible: root.account.isHDWallet
            collapsed: true

            Item {
                width: parent.width
                implicitHeight: {
                    if (root.account.isDecrypted)
                        return grid.height + helpText.height + warningText.height + 20
                    return infoText.height
                }
                GridLayout {
                    id: grid
                    visible: root.account.isDecrypted
                    width: parent.width
                    columns: 2
                    Label {
                        text: qsTr("Seed-phrase") + ":"
                    }
                    TextArea {
                        readOnly: true
                        text: root.account.mnemonic
                        Layout.fillWidth: true
                        selectByMouse: true
                        mouseSelectionMode: TextEdit.SelectWords
                        wrapMode: TextEdit.WordWrap
                        padding: 0
                    }
                    Label {
                        text: qsTr("Derivation") + ":"
                    }
                    Flowee.LabelWithClipboard {
                        text: root.account.hdDerivationPath
                    }
                }
                Label {
                    id: helpText
                    visible: grid.visible
                    width: parent.width
                    anchors.top: grid.bottom
                    anchors.topMargin: 10
                    text: qsTr("Please save the seed-phrase on paper, in the right order, with the derivation path. This seed will allow you to recover your wallet in case of computer failure.")
                    textFormat: Text.StyledText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Label {
                    id: warningText
                    visible: grid.visible
                    width: parent.width
                    anchors.top: helpText.bottom
                    anchors.topMargin: 10
                    text: qsTr("<b>Important</b>: Never share your seed-phrase with others!")
                    textFormat: Text.StyledText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Label {
                    id: infoText
                    visible: !root.account.isDecrypted
                    width: parent.width
                    text: qsTr("This wallet is protected by password (pin-to-pay). To see the backup details you need to provide the password.")
                    textFormat: Text.StyledText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }
    }
    Keys.forwardTo: Flowee.ListViewKeyHandler {
        target: historyView
    }
}
