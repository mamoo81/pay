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
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import Flowee.org.pay

Page {
    id: importAccount
    headerText: qsTr("Import Wallet")

    property var typedData: Pay.identifyString(secrets.totalText)
    property bool finished: typedData === Wallet.PrivateKey || ((typedData === Wallet.CorrectMnemonic
                                                                 || typedData === Wallet.ElectrumMnemonic)
                                                                && derivationPath.derivationOk);
    property bool isMnemonic: typedData === Wallet.CorrectMnemonic || typedData === Wallet.PartialMnemonic || typedData === Wallet.PartialMnemonicWithTypo || typedData === Wallet.ElectrumMnemonic;
    property bool isElectrumMnemonic: typedData === Wallet.ElectrumMnemonic
    property bool isPrivateKey: typedData === Wallet.PrivateKey

    Flickable {
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        contentWidth: parent.width
        contentHeight: column.height

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 10
            Flowee.Label {
                text: qsTr("Please enter the secrets of the wallet to import. This can be a seed-phrase or a private key.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            PageTitledBox {
                title: qsTr("Secret", "The seed-phrase or private key")
                Item {
                    width: parent.width
                    height: secrets.height + 10
                    Flowee.MultilineTextField {
                        id: secrets
                        focus: true
                        nextFocusTarget: accountName
                        clip: true
                        anchors.left: parent.left
                        anchors.right: feedback.left
                    }
                    Flowee.Label {
                        id: feedback
                        text: importAccount.finished ? "✔" : "  "
                        color: Pay.useDarkSkin ? "#37be2d" : "green"
                        font.pixelSize: 24
                        anchors.right: startScan.left
                    }
                    Flowee.ImageButton {
                        id: startScan
                        source: "qrc:/qr-code-scan" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
                        onClicked: scanner.start();
                        iconSize: 25
                        anchors.verticalCenter: feedback.verticalCenter
                        anchors.right: parent.right

                        QRScanner {
                            id: scanner
                            scanType: QRScanner.Seed
                            onFinished: if (scanResult !== "") secrets.text = scanResult;
                        }
                    }
                }
            }

            Flowee.Label {
                id: detectedType
                color: typedData === Wallet.PartialMnemonicWithTypo ? mainWindow.errorRed : feedback.color
                text: {
                    var typedData = importAccount.typedData
                    if (typedData === Wallet.PrivateKey)
                        return qsTr("Private key", "description of type") // TODO print address to go with it
                    if (typedData === Wallet.CorrectMnemonic) {
                        if (forceElectrum.checked)
                            return qsTr("BIP 39 seed-phrase (interpreted as Electrum)", "description of type")
                        return qsTr("BIP 39 seed-phrase", "description of type")
                    }
                    if (typedData === Wallet.ElectrumMnemonic)
                        return qsTr("Electrum seed-phrase", "description of type")
                    if (typedData === Wallet.PartialMnemonicWithTypo)
                        return qsTr("Unrecognized word", "Word from the seed-phrases lexicon")
                    if (typedData === Wallet.MissingLexicon)
                        return "Installation error; no lexicon found"; // intentionally not translated, end-users should not see this
                    return ""
                }
            }

            PageTitledBox {
                title: qsTr("Name")
                Flowee.TextField {
                    id: accountName
                    width: parent.width
                }
            }

            Flowee.CheckBox {
                id: singleAddress
                text: qsTr("Force Single Address");
                toolTipText: qsTr("When enabled, no extra addresses will be auto-generated in this wallet.\nChange will come back to the imported key.")
                checked: true
                visible: importAccount.isPrivateKey
            }
            Flowee.CheckBox {
                id: forceElectrum
                text: qsTr("Force Electrum");
                toolTipText: qsTr("When enabled, the seed phrase specified will be force-interpreted as an Electrum and/or Electron Cash phrase.")
                checked: importAccount.isElectrumMnemonic
                visible: importAccount.isMnemonic
                enabled: importAccount.isMnemonic && !importAccount.isElectrumMnemonic
                property string prevDerPath: ""
                onCheckedChanged: {
                    derivationLabel.enabled = !checked;
                    derivationPath.enabled = !checked;
                    if (checked) {
                        // Electrum mnemonics always use derivation path: "m/" and never anything else.
                        prevDerPath = derivationPath.text;
                        derivationPath.text = "m/";
                    } else if (prevDerPath) {
                        derivationPath.text = prevDerPath;
                    }
                }
            }

            PageTitledBox {
                title: qsTr("Oldest Transaction")
                Flow {
                    width: parent.width
                    spacing: 10
                    Flowee.ComboBox {
                        id: month
                        model: {
                            let locale = Qt.locale();
                            var list = [];
                            for (let i = QQC2.Calendar.January; i <= QQC2.Calendar.December; ++i) {
                                list.push(locale.monthName(i));
                            }
                            return list;
                        }
                        width: implicitWidth * 1.3 // this makes it fit for bigger fonts.
                    }
                    Flowee.ComboBox {
                        id: year
                        model: {
                            var list = [];
                            let last = new Date().getFullYear();
                            for (let i = 2010; i <= last; ++i) {
                                list.push(i);
                            }
                            return list;
                        }
                        currentIndex: 9;
                    }
                }
            }

            PageTitledBox {
                id: derivationLabel
                title: qsTr("Derivation")
                visible: !importAccount.isPrivateKey
                Flowee.TextField {
                    id: derivationPath
                    property bool derivationOk: Pay.checkDerivation(text);
                    width: parent.width
                    text: "m/44'/0'/0'" // What most BCH wallets are created with
                    color: derivationOk ? palette.text : "red"
                }
            }
            PageTitledBox {
                title: qsTr("Alternate phrase")
                visible: !importAccount.isPrivateKey
                Flowee.TextField {
                    // according to the BIP39 spec this is the 'password', but from a UX
                    // perspective that is confusing and no actual wallet uses it
                    id: passwordField
                    width: parent.width
                }
            }

            Item {
                width: parent.width
                height: createButton.implicitHeight
                Flowee.Button {
                    id: createButton
                    enabled: finished
                    anchors.right: parent.right
                    text: qsTr("Create")
                    onClicked: {
                        var sh = new Date(year.currentIndex + 2010, month.currentIndex, 1);
                        if (importAccount.isMnemonic)
                            var options = Pay.createImportedHDWallet(secrets.text, passwordField.text, derivationPath.text, accountName.text, sh, forceElectrum.checked);
                        else
                            options = Pay.createImportedWallet(secrets.text, accountName.text, sh)

                        options.forceSingleAddress = singleAddress.checked;

                        var accounts = portfolio.accounts;
                        for (var i = 0; i < accounts.length; ++i) {
                            var a = accounts[i];
                            if (a.name === options.name) {
                                portfolio.current = a;
                                break;
                            }
                        }
                        thePile.pop();
                        thePile.pop();
                    }
                }
            }
        }
    }
}
