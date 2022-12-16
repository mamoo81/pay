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

        Item {
            id: nameRow
            width: parent.width
            height: accountName.height + 10
            y: 5

            Flowee.Label {
                id: accountName
                text: account.name
                font.pixelSize: root.font.pixelSize * 1.1
            }

            Rectangle {
                width: 20
                visible: false // TODO make this work.
                height: 20
                anchors.left: accountName.width > root.width ? accountName.left : accountName.right

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -10
                    // onClicked: // TODO start editing the name
                }
            }
        }

        Flowee.AccountTypeLabel {
            Layout.fillWidth: true
            account: root.account
        }
        TextButton {
            Layout.fillWidth: true
            text: qsTr("Primary Wallet")
            onClicked: root.account.isDefaultWallet = !root.account.isDefaultWallet

            Flowee.CheckBox {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                checked: root.account.isDefaultWallet
                onClicked: root.account.isDefaultWallet = checked
            }
        }

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
        }
        /*
          TODO Give opportunity to decrypt here.
         */

        /*
          TODO archived wallets functionality
         */

        TextButton {
            text: qsTr("Wallet Settings")
            showPageIcon: true
            Layout.fillWidth: true
            onClicked: {} // TODO
        }

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
                        Flowee.Label { text: "xpub" }
                        Flowee.LabelWithClipboard {
                            id: xpub
                            Layout.fillWidth: true
                            text: root.account.xpub
                        }
                        VisualSeparator { }
                        Flowee.Label {
                            text: qsTr("Wallet seed-phrase")
                        }
                        Flowee.LabelWithClipboard {
                            Layout.fillWidth: true
                            text: root.account.mnemonic
                            wrapMode: Text.Wrap
                        }
                        VisualSeparator { }
                        Flowee.Label { text: qsTr("Derivation Path") }
                        Flowee.LabelWithClipboard { text: root.account.hdDerivationPath }
                    }
                }
            }

            Component {
                id: backupDetails
                Page {
                    headerText: qsTr("Backup Details")
                    ColumnLayout {
                        width: parent.width
                        Flowee.Label { text: "TODO" }
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
