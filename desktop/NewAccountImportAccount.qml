import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "widgets" as Flowee
import Flowee.org.pay 1.0

GridLayout {
    id: importAccount
    columns: 3
    rowSpacing: 10

    property var typedData: Pay.identifyString(secrets.text)
    property bool finished: typedData === Bitcoin.PrivateKey || typedData === Bitcoin.CorrectMnemonic;
    property bool isMnemonic: typedData === Bitcoin.CorrectMnemonic || typedData === Bitcoin.PartialMnemonic || typedData === Bitcoin.PartialMnemonicWithTypo;
    property bool isPrivateKey: typedData === Bitcoin.PrivateKey

    Label {
        text: qsTr("Please enter the secrets of the wallet to import. This can be a seedphrase or a private key.")
        Layout.columnSpan: 3
        wrapMode: Text.Wrap
    }
    Label {
        text: qsTr("Secret:", "The private phrase or key")
        Layout.alignment: Qt.AlignBaseline
    }
    Flowee.MultilineTextField {
        id: secrets
        Layout.fillWidth: true
        onTextChanged: if (!importAccount.isMnemonic) { text = text.trim(); }
        nextFocusTarget: accountName
        placeholderText: qsTr("Example; %1", "placeholder text").arg("L5bxhjPeQqVFgCLALiFaJYpptdX6Nf6R9TuKgHaAikcNwg32Q4aL")
    }
    Label {
        id: feedback
        text: importAccount.finished ? "✔" : " "
        color: Bitcoin.useDarkSkin ? "#37be2d" : "green"
        font.pixelSize: 24
        Layout.alignment: Qt.AlignTop
    }
    Label {
        text: qsTr("Name:");
        Layout.alignment: Qt.AlignBaseline
    }
    Flowee.TextField {
        id: accountName
        onAccepted: if (startImport.enabled) startImport.clicked()
        Layout.columnSpan: 2
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.columnSpan: 3

        Label {
            id: detectedType
            color: typedData === Pay.PartialMnemonicWithTypo ? "red" : feedback.color
            text: {
                var typedData = importAccount.typedData
                if (typedData === Bitcoin.PrivateKey)
                    return qsTr("Private key", "description of type") // TODO print address to go with it
                if (typedData === Bitcoin.CorrectMnemonic)
                    return qsTr("BIP 39 mnemonic", "description of type")
                if (typedData === Bitcoin.PartialMnemonicWithTypo)
                    return qsTr("Unrecognized word", "Word from the seed-phrases lexicon")
                if (typedData === Bitcoin.MissingLexicon)
                    return "Installation error; no lexicon found"; // intentionally not translated, end-users should not see this
                return ""
            }
        }
        Item { width: 1; height: 1; Layout.fillWidth: true } // spacer
        Flowee.Button {
            id: startImport
            enabled: importAccount.finished

            text: qsTr("Import wallet")
            onClicked: {
                var sh = parseInt("0" + startHeight.text, 10);
                if (sh === 0) // the genesis was block 1, zero doesn't exist
                    sh = 1;
                if (importAccount.isMnemonic)
                    var options = Pay.createImportedHDWallet(secrets.text, passwordField.text, derivationPath.text, accountName.text, sh);
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
                root.visible = false;
            }
        }
    }

    Flowee.GroupBox {
        title: qsTr("Advanced Options")
        collapsed: true
        Layout.columnSpan: 3
        Layout.fillWidth: true

        columns: 2
        /*
        Flowee.CheckBox {
            id: schnorr
            text: qsTr("Default to signing using ECDSA");
            tooltipText: qsTr("When enabled, newer style Schnorr signatures are not set as default for this wallet.")
            Layout.columnSpan: 2
        } */
        Flowee.CheckBox {
            id: singleAddress
            text: qsTr("Force Single Address");
            tooltipText: qsTr("When enabled, no extra addresses will be auto-generated in this wallet.\nChange will come back to the imported key.")
            checked: true
            visible: importAccount.isPrivateKey
            Layout.columnSpan: 2
        }
        Label {
            text: qsTr("Alternate phrase:")
            visible: !importAccount.isPrivateKey
        }
        Flowee.TextField {
            // according to the BIP39 spec this is the 'password', but from a UX
            // perspective that is confusing and no actual wallet uses it
            id: passwordField
            visible: !importAccount.isPrivateKey
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Start Height:")
        }
        Flowee.TextField {
            id: startHeight
            Layout.fillWidth: true
            validator: IntValidator{bottom: 0; top: 999999}
        }
        Label {
            text: qsTr("Derivation:")
            visible: !importAccount.isPrivateKey
        }
        Flowee.TextField {
            id: derivationPath
            text: "m/44'/145'/0'" // default for BCH wallets
            visible: !importAccount.isPrivateKey
            color: Pay.checkDerivation(text) ? palette.text : "red"
        }
    }
}
