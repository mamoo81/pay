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
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;

QQC2.Control {
    id: root
    height: column.height

    property QtObject account: null

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 10
        Flowee.AccountTypeLabel {
            Layout.fillWidth: true
            account: root.account
            font.pixelSize: root.font.pixelSize * 0.8
            color: palette.brightText
        }
        PageTitledBox {
            title: singleAccountSetup ? qsTr("Name") : ""
            EditableLabel {
                text: account.name
                onEdited: root.account.name = text
                width: parent.width
                // this hides the edit icon for a smoother swipe
                editable: index === accountPageListView.currentIndex
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: "#00000000"
            border.width: 3
            border.color: Pay.useDarkSkin ? "#b39554" : "#e5be6b"
            visible: root.account.isArchived
            radius: 10
            implicitHeight: archivedWarningLabel.implicitHeight + 20

            Flowee.Label {
                id: archivedWarningLabel
                y: 10
                width: parent.width
                horizontalAlignment: Qt.AlignHCenter
                text: qsTr("Archived wallets do not check for activities. Balance may be out of date");
                font.bold: true
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }

        InstaPayConfigButton {
            visible: !root.account.isArchived
            account: root.account
        }

        TextButton {
            text: qsTr("Backup information")
            showPageIcon: true
            Layout.fillWidth: true
            enabled: root.account.isDecrypted
            onClicked: thePile.push(root.account.isHDWallet ? hdBackupDetails : backupDetails);

            Component {
                id: hdBackupDetails
                Page {
                    id: detailsPage
                    headerText: qsTr("Backup Details")

                    Item {
                        // non-layoutable items.

                        QQC2.Popup {
                            id: seedPopup
                            width: 260
                            height: 260
                            x: 55
                            y: 100
                            modal: true
                            closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent
                            background: Rectangle {
                                color: palette.light
                                border.color: palette.midlight
                                border.width: 1
                                radius: 5
                            }
                            Flowee.QRWidget {
                                id: seedQr
                                qrSize: 250
                                textVisible: false
                                useRawString: true
                            }
                        }
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 10

                        PageTitledBox {
                            title: qsTr("Wallet seed-phrase")

                            Item {
                                implicitHeight: mnemonicLabel.implicitHeight
                                width: parent.width
                                Flowee.LabelWithClipboard {
                                    id: mnemonicLabel
                                    text: root.account.mnemonic
                                    width: parent.width - 36
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                    font.wordSpacing: 1
                                }
                                Image {
                                    width: 20
                                    height: 20
                                    anchors.right: parent.right
                                    source: "qrc:/qr-code" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            seedQr.qrText = root.account.mnemonic
                                            seedPopup.open();
                                        }
                                    }
                                }
                            }
                        }

                        PageTitledBox {
                            title: qsTr("Seed format")
                            visible: root.account.isElectrumMnemonic
                            Item {
                                implicitHeight: electrumLabel.implicitHeight
                                width: parent.width
                                Flowee.Label {
                                    id: electrumLabel
                                    text: "Electrum"
                                    width: parent.width - 36
                                    font.italic: true
                                    font.wordSpacing: 1
                                }
                            }
                        }

                        PageTitledBox {
                            title: qsTr("Starting Height", "height refers to block-height")
                            Flowee.LabelWithClipboard { text: root.account.initialBlockHeight }
                        }

                        PageTitledBox {
                            title: qsTr("Derivation Path")
                            Flowee.LabelWithClipboard {
                                text: root.account.hdDerivationPath
                            }
                        }

                        PageTitledBox {
                            title: qsTr("xpub")

                            Flowee.LabelWithClipboard {
                                text: root.account.xpub
                                width: parent.width
                                wrapMode: Text.WrapAnywhere
                            }
                        }

                        Flowee.Label {
                            Layout.fillWidth: true
                            text: qsTr("Please save the seed-phrase on paper, in the right order, with the derivation path. This seed will allow you to recover your wallet in case you lose your mobile.")
                            textFormat: Text.StyledText
                            font.italic: true
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }
                        Flowee.Label {
                            Layout.fillWidth: true
                            text: qsTr("<b>Important</b>: Never share your seed-phrase with others!")
                            font.italic: true
                            textFormat: Text.StyledText
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }
                    }
                }
            }

            Component {
                id: backupDetails
                Page {
                    headerText: qsTr("Wallet keys")

                    property QtObject showIndexAction: QQC2.Action {
                        text: qsTr("Show Index", "toggle to show numbers")
                        checkable: true
                        checked: listView.showHdIndex
                        onTriggered: listView.showHdIndex = checked
                    }

                    menuItems: {
                        if (root.account.isHDWallet)
                            return [showIndexAction];
                        return [];
                    }

                    PageTitledBox {
                        id: optionsBox
                        Flowee.CheckBox {
                            width: parent.width
                            text: qsTr("Change Addresses")
                            visible: root.account.isHDWallet
                            onClicked: root.account.secrets.showChangeChain = checked
                            toolTipText: qsTr("Switches between addresses others can pay you on, and addresses the wallet uses to send change back to yourself.")
                        }
                        Flowee.CheckBox {
                            width: parent.width
                            text: qsTr("Used Addresses");
                            visible: !root.account.isSingleAddressAccount
                            onClicked: root.account.secrets.showUsedAddresses = checked
                            toolTipText: qsTr("Switches between still in use addresses and formerly used, new empty, addresses")
                        }
                    }

                    Item {
                        // this is a horrible hack...
                        // First, ListViews almost always require clipping on,
                        // otherwise list-items can overlap the rest of your view.
                        // But if I enable clipping I no longer get the nice
                        // width-filling backgrounds...
                        // Sooo. I need a clipping item that is full width (negative
                        // left and right margin)
                        id: clipItem
                        anchors {
                            top: optionsBox.bottom
                            topMargin: 10
                            bottom: parent.bottom
                            left: parent.left
                            right: parent.right
                            leftMargin: -10
                            rightMargin: -10
                        }
                        clip: true
                        Flowee.WalletSecretsView {
                            id: listView
                            anchors {
                                fill: parent
                                leftMargin: 10
                                rightMargin: 10
                            }
                            account: root.account
                            showHdIndex: false
                        }
                    }
                }
            }
        }

        /*
        TextButton {
            visible: !root.account.needsPinToOpen
            showPageIcon: true
            text: {
                if (!root.account.needsPinToPay)
                    return qsTr("Enable Pin to Pay")
                if (!root.account.needsPinToOpen)
                    return qsTr("Convert to Pin to Open")
                return ""; // already fully encrypted
            }
            subtext: {
                if (root.account.needsPinToPay)
                    return qsTr("Pin to Pay is enabled");
                return "Wallet is not protected";
            }

            onClicked: {} // TODO
        }

        Rectangle {
            id: decryptButton
            height: decryptButtonText.height +20
            width: decryptButtonText.width + 30
            visible: !root.account.isDecrypted
            radius: 5
            color: Pay.useDarkSkin ? "#c2cacc" : "#bcc3c5"
            Text {
                id: decryptButtonText
                text: qsTr("Open Wallet")
                anchors.centerIn: parent
                color: "black"
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.ArrowCursor
                // TODO Give opportunity to decrypt here.
            }
        }
        */

        TextButton {
            text: qsTr("Addresses and keys")
            visible: root.account.isHDWallet
            enabled: !root.account.needsPinToOpen || root.account.isDecrypted
            showPageIcon: true
            Layout.fillWidth: true
            onClicked: thePile.push(backupDetails);
        }
        PageTitledBox {
            title: qsTr("Sync Status")

            Flowee.Label {
                id: syncLabel
                width: parent.width
                property string time: ""
                text: {
                    if (root.account.isArchived)
                        return root.account.timeBehind;

                    var height = root.account.lastBlockSynched
                    if (height < 1)
                        return ""
                    var time = "";
                    if (syncLabel.time !== "")
                        time = "  (" + syncLabel.time + ")";
                    return height + " / " + Pay.chainHeight + time;
                }
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                Timer {
                    // the lastBlockSynchedTime does not change,
                    // but since we render it as '12 minutes ago'
                    // we need to actually re-interpret that
                    // ever so often to keep the relative time.
                    running: !root.account.isArchived
                    interval: 30000 // 30 sec
                    repeat: true
                    triggeredOnStart: true
                    onTriggered: {
                        syncLabel.time = Pay.formatDateTime(
                                    root.account.lastBlockSynchedTime);
                    }
                }
            }
        }
        Flowee.CheckBox {
            Layout.fillWidth: true
            visible: !singleAccountSetup && !root.account.isArchived
            checked: root.account.countBalance === false
            text: qsTr("Hide balance in overviews")
            onClicked: root.account.countBalance = !checked
        }
        Flowee.CheckBox {
            Layout.fillWidth: true
            visible: !singleAccountSetup && !root.account.isArchived
            checked: root.account.isPrivate
            text: qsTr("Hide in private mode")
            onClicked: root.account.isPrivate = checked
        }

        Item { width: 1; height: 10 } // spacer. Make the button not to close to the clickable checkbox for fatfingered people.

        Rectangle {
            id: archiveButton
            height: archiveButtonText.height + 20
            color: Pay.useDarkSkin ? "#b39554" : "#e5be6b"
            width: archiveButtonText.width + 30
            visible: !singleAccountSetup

            Text {
                id: archiveButtonText
                text: root.account.isArchived ? qsTr("Unarchive Wallet") : qsTr("Archive Wallet")
                anchors.centerIn: parent
                color: "black"
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.ArrowCursor
                onClicked: root.account.isArchived = !root.account.isArchived
            }
        }
    }
}
