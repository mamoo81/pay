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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "widgets" as Flowee

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

    Flickable {
        id: scrollablePage
        anchors.top: basicProperties.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        clip: true

        contentHeight: detailsPane.height + 10 + addressesList.height + 10

        ColumnLayout {
            id: detailsPane
            spacing: 10
            width: parent.width - 20
            x: 10

            Label {
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
            Label {
                id: walletType
                visible: root.account.isDecrypted || root.account.needsPinToPay
                font.italic: true
                text: {
                    if (root.account.isSingleAddressAccount)
                        return qsTr("This wallet is a single-address wallet.")

                    if (root.account.isHDWallet)
                        return qsTr("This wallet is based on a HD seed-phrase")

                     // ok we only have one more type so far, so this is rather simple...
                    return qsTr("This wallet is a simple multiple-address wallet.")
                }
            }
            RowLayout {
                width: parent.width
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
                    clipboardText: text
                    menuText: qsTr("Copy")
                }
            }

    /* TODO, features to add;
            Label {
                text: qsTr("Security:")
                Layout.columnSpan: 3
                font.italic: true
            }

            Flowee.CheckBox {
                id: schnorr
                checked: true
                text: qsTr("Use Schnorr signatures");
            }
            Flowee.CheckBox {
                id: syncOnStart
                checked: false
                text: qsTr("Sync on Start");
            }
            Flowee.CheckBox {
                id: useIndexServer
                checked: false
                text: qsTr("Use Indexing Server");
            }
    */
        }

        Flowee.GroupBox {
            id: addressesList
            width: parent.width
            anchors.top: detailsPane.bottom
            anchors.topMargin: 20
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
            ListView {
                id: historyView
                model: root.account.secrets
                Layout.fillWidth: true
                implicitHeight: root.account.isSingleAddressAccount ? contentHeight : scrollablePage.height / 10 * 7
                clip: true
                delegate: Rectangle {
                    id: delegateRoot
                    color: (index % 2) == 0 ? mainWindow.palette.base : mainWindow.palette.alternateBase
                    width: ListView.view.width
                    height: addressLabel.height + 10 + amountLabel.height + 10

                    Label {
                        text: hdIndex
                        anchors.baseline: addressLabel.baseline
                        anchors.right: addressLabel.left
                        anchors.rightMargin: 10
                        visible: root.account.isHDWallet
                    }

                    Flowee.LabelWithClipboard {
                        id: addressLabel
                        y: 5
                        x: root.account.isHDWallet ? 50 : 0
                        text: address
                    }

                    Flowee.BitcoinAmountLabel {
                        id: amountLabel
                        value: balance
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12
                    }
                    Label {
                        id: coinCountLabel
                        anchors.left: parent.left
                        anchors.leftMargin: delegateRoot.width / 4
                        anchors.baseline: amountLabel.baseline
                        text: qsTr("Coins: %1 / %2").arg(numCoins).arg(historicalCoinCount)
                    }
                    Label {
                        id: schnorrIndicator
                        anchors.left: coinCountLabel.right
                        anchors.leftMargin: 10
                        anchors.baseline: amountLabel.baseline
                        visible: usedSchnorr
                        text: "ⓢ"
                        MouseArea {
                            id: mousy
                            anchors.fill: parent
                            anchors.margins: -10
                            hoverEnabled: true

                            ToolTip {
                                parent: schnorrIndicator
                                text: qsTr("Signed with Schnorr signatures in the past")
                                delay: 700
                                visible: mousy.containsMouse
                            }
                        }
                    }

                    ConfigItem {
                        id: button
                        anchors.right: parent.right
                        wide: true
                        y: 5
                        property QtObject copyAddress: Action {
                            text: qsTr("Copy Address")
                            onTriggered: Pay.copyToClipboard(address)
                        }
                        property QtObject copyPrivKey: Action {
                            text: qsTr("Copy Private Key")
                            onTriggered: Pay.copyToClipboard(privatekey)
                        }
                        /*
                        Action {
                            text: qsTr("Move to New Wallet")
                            onTriggered: ;
                        } */
                        onAboutToOpen: {
                            var items = [];
                            items.push(copyAddress);
                            if (root.account.isDecrypted)
                                items.push(copyPrivKey);
                            setMenuActions(items)
                        }
                    }
                }
                ScrollBar.vertical: Flowee.ScrollThumb {
                    id: thumb
                    minimumSize: 20 / activityView.height
                    visible: size < 0.9
                }

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
                        menuText: qsTr("Copy")
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
