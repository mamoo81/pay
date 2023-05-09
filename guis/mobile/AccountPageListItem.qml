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
        }

        PageTitledBox {
            title: qsTr("Sync Status")

            Flowee.Label {
                text: {
                    var height = root.account.lastBlockSynched
                    if (height < 1)
                        return ""
                    var time = Pay.formatDateTime(root.account.lastBlockSynchedTime);
                    if (time !== "")
                        time = "  (" + time + ")";
                    return height + " / " + Pay.chainHeight + time;
                }
            }
        }

        PageTitledBox {
            title: qsTr("Wallet Name")

            Flowee.TextField {
                text: account.name
                onTextChanged: root.account.name = text
                width: parent.width
            }
        }

        PageTitledBox {
            title: qsTr("Options")

            Flowee.CheckBox {
                visible: !portfolio.singleAccountSetup
                enabled: !root.account.isArchived
                checked: root.account.isPrimaryAccount
                onClicked: root.account.isPrimaryAccount = checked
                text: qsTr("Primary Wallet")
            }
        }

        /*
        TextButton {
            Layout.fillWidth: true
            visible: !root.account.needsPinToOpen
            showPageIcon: true
            text: {
                if (!root.account.needsPinToPay)
                    return qsTr("Enable Pin to Pay")
                if (!root.account.needsPinToOpen)
                    return qsTr("Enable Pin to Open")
                return ""; // already fully encrypted
            }
            subtext: {
                if (root.account.needsPinToPay)
                    return qsTr("Pin to Pay is enabled");
                return "";
            }

            onClicked: {} // TODO
        } */
        /*
          TODO Give opportunity to decrypt here.
         */

        /*
          TODO archived wallets functionality
         */

        TextButton {
            text: qsTr("Backup information")
            showPageIcon: true
            Layout.fillWidth: true
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
                            text: qsTr("Change Addresses")
                            visible: root.account.isHDWallet
                            onClicked: root.account.secrets.showChangeChain = checked
                            tooltipText: qsTr("Switches between addresses others can pay you on, and addresses the wallet uses to send change back to yourself.")
                        }
                        Flowee.CheckBox {
                            text: qsTr("Used Addresses");
                            visible: !root.account.isSingleAddressAccount
                            onClicked: root.account.secrets.showUsedAddresses = checked
                            tooltipText: qsTr("Switches between still in use addresses and formerly used, new empty, addresses")
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
        TextButton {
            text: qsTr("Addresses and Keys")
            visible: root.account.isHDWallet
            showPageIcon: true
            Layout.fillWidth: true
            onClicked: thePile.push(backupDetails);
        }
    }


   /*
    Q_PROPERTY(int lastBlockSynched READ lastBlockSynched NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(int initialBlockHeight READ initialBlockHeight NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(QDateTime lastBlockSynchedTime READ lastBlockSynchedTime NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(QString timeBehind READ timeBehind NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(WalletSecretsModel* secrets READ secretsModel NOTIFY modelsChanged)
    Q_PROPERTY(bool isArchived READ isArchived WRITE setIsArchived NOTIFY isArchivedChanged)
    Q_PROPERTY(QDateTime lastMinedTransaction READ lastMinedTransaction NOTIFY balanceChanged)
    Q_PROPERTY(bool isDecrypted READ isDecrypted NOTIFY encryptionChanged)
    */


}
