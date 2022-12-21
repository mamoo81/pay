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

        Flowee.AccountTypeLabel {
            Layout.fillWidth: true
            account: root.account
        }
        Flowee.TextField {
            text: account.name
            onTextChanged: root.account.name = text
            Layout.fillWidth: true
        }
        TextButton {
            visible: root.account.isUserOwned
            Layout.fillWidth: true
            text: qsTr("Primary Wallet")
            onClicked: if (!root.account.isArchived) root.account.isPrimaryAccount = !root.account.isPrimaryAccount

            Flowee.CheckBox {
                enabled: !root.account.isArchived
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                checked: root.account.isPrimaryAccount
                onClicked: root.account.isPrimaryAccount = checked
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

        /* // TODO
        TextButton {
            text: qsTr("Wallet Settings")
            showPageIcon: true
            Layout.fillWidth: true
            onClicked: {}
        }*/

        TextButton {
            text: qsTr("Backup information")
            showPageIcon: true
            Layout.fillWidth: true
            onClicked: thePile.push(root.account.isHDWallet ? hdBackupDetails : backupDetails);

            Component {
                id: hdBackupDetails
                Page {
                    headerText: qsTr("Backup Details")
                    ColumnLayout {
                        width: parent.width
                        Flowee.Label { text: "xpub" + ":" }
                        Flowee.LabelWithClipboard {
                            id: xpub
                            Layout.fillWidth: true
                            text: root.account.xpub
                        }
                        VisualSeparator { }

                        Flowee.Label {
                            text: qsTr("Wallet seed-phrase") + ":"
                        }
                        // TODO allow showing the seed phrase as a QR
                        Flowee.LabelWithClipboard {
                            Layout.fillWidth: true
                            text: root.account.mnemonic
                            wrapMode: Text.Wrap
                        }
                        VisualSeparator { }
                        Flowee.Label { text: qsTr("Derivation Path") + ":" }
                        Flowee.LabelWithClipboard { text: root.account.hdDerivationPath }
                        VisualSeparator { }
                        Flowee.Label {
                            Layout.fillWidth: true
                            text: qsTr("Please save the seed-phrase on paper, in the right order, with the derivation path. This seed will allow you to recover your wallet in case you lose your mobile.")
                            textFormat: Text.StyledText
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }
                        Flowee.Label {
                            Layout.fillWidth: true
                            text: qsTr("<b>Important</b>: Never share your seed-phrase with others!")
                            textFormat: Text.StyledText
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }
                    }
                }
            }

            Component {
                id: backupDetails
                Page {
                    headerText: qsTr("Backup Details")
                    Flowee.CheckBox {
                        id: usedAddresses
                        anchors.top: parent.top
                        width: parent.width
                        text: qsTr("Used Addresses");
                        visible: !root.account.isSingleAddressAccount
                        onClicked: root.account.secrets.showUsedAddresses = checked
                        tooltipText: qsTr("Switches between unused and used Bitcoin addresses")
                    }

                    Flowee.WalletSecretsView {
                        anchors.top: usedAddresses.visible ? usedAddresses.bottom : parent.top
                        anchors.topMargin: 10
                        anchors.bottom: parent.bottom
                        width: parent.width
                        account: root.account
                        /*
                        ScrollBar: {
                            id: thumb
                            minimumSize: 20 / activityView.height
                            visible: size < 0.9
                        }*/
                        clip: true
                    }
                }
            }
        }
    }


   /*
    Q_PROPERTY(double balanceConfirmed READ balanceConfirmed NOTIFY balanceChanged)
    Q_PROPERTY(double balanceUnconfirmed READ balanceUnconfirmed NOTIFY balanceChanged)
    Q_PROPERTY(double balanceImmature READ balanceImmature NOTIFY balanceChanged)

    Q_PROPERTY(int unspentOutputCount READ unspentOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int historicalOutputCount READ historicalOutputCount NOTIFY utxosChanged)
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
