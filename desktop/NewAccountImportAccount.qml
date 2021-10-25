import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import Flowee.org.pay 1.0

GridLayout {
    id: importAccount
    columns: 3
    rowSpacing: 10

    property var typedData: Flowee.identifyString(secrets.text)
    property bool finished: typedData === Pay.PrivateKey || typedData === Pay.CorrectMnemonic;
    property bool isMnemonic: typedData === Pay.CorrectMnemonic || typedData === Pay.PartialMnemonic;

    Label {
        text: qsTr("Please enter the secrets of the wallet to import. This can be a seedphrase or a private key.")
        Layout.columnSpan: 3
        wrapMode: Text.Wrap
    }
    Label {
        text: qsTr("Secret:", "The private phrase or key")
        Layout.alignment: Qt.AlignBaseline
    }
    FloweeMultilineTextField {
        id: secrets
        Layout.fillWidth: true
        onTextChanged: if (!importAccount.isMnemonic) { text = text.trim(); }
        nextFocusTarget: accountName
        placeholderText: qsTr("Example; %1", "placeholder text").arg("L5bxhjPeQqVFgCLALiFaJYpptdX6Nf6R9TuKgHaAikcNwg32Q4aL")
    }
    Label {
        id: feedback
        text: importAccount.finished ? "✔" : " "
        color: Flowee.useDarkSkin ? "#37be2d" : "green"
        font.pixelSize: 24
        Layout.alignment: Qt.AlignTop
    }
    Label {
        text: qsTr("Name:");
        Layout.alignment: Qt.AlignBaseline
    }
    FloweeTextField {
        id: accountName
        onAccepted: if (startImport.enabled) startImport.clicked()
        Layout.columnSpan: 2
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.columnSpan: 3

        Label {
            id: detectedType
            color: feedback.color
            text: {
                var typedData = importAccount.typedData
                if (typedData === Pay.PrivateKey)
                    return qsTr("Private key", "description of type") // TODO print address to go with it
                if (typedData === Pay.CorrectMnemonic)
                    return qsTr("BIP 39 mnemonic", "description of type")
                return ""
            }
        }
        Item { width: 1; height: 1; Layout.fillWidth: true } // spacer
        Button2 {
            id: startImport
            enabled: importAccount.finished

            text: qsTr("Import wallet")
            onClicked: {
                var sh = parseInt("0" + startHeight.text, 10);
                if (sh === 0) // the genesis was block 1, zero doesn't exist
                    sh = 1;
                if (importAccount.isMnemonic)
                    var options = Flowee.createImportedHDWallet(secrets.text, passwordField.text, derivationPath.text, accountName.text, sh);
                else
                    options = Flowee.createImportedWallet(secrets.text, accountName.text)

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

    FloweeGroupBox {
        title: qsTr("Advanced Options")
        isCollapsed: true
        Layout.columnSpan: 3
        Layout.fillWidth: true

        columns: 2
        /*
        FloweeCheckBox {
            id: schnorr
            text: qsTr("Default to signing using ECDSA");
            tooltipText: qsTr("When enabled, newer style Schnorr signatures are not set as default for this wallet.")
            Layout.columnSpan: 2
        } */
        FloweeCheckBox {
            id: singleAddress
            text: qsTr("Force Single Address");
            tooltipText: qsTr("When enabled, this wallet will be limited to one address.\nThis ensures only one private key will need to be backed up")
            visible: !importAccount.isMnemonic
            Layout.columnSpan: 2
        }
        Label {
            text: qsTr("Password:")
            visible: importAccount.isMnemonic
        }
        FloweeTextField {
            id: passwordField
            visible: importAccount.isMnemonic
            Layout.fillWidth: true
            echoMode: TextInput.Password
            // TODO allow showing the text in the textfield component
        }
        Label {
            text: qsTr("Start Height:")
        }
        FloweeTextField {
            id: startHeight
            Layout.fillWidth: true
            validator: IntValidator{bottom: 0; top: 999999}
        }
        Label {
            text: qsTr("Derivation:")
            visible: importAccount.isMnemonic
        }
        FloweeTextField {
            id: derivationPath
            text: "m/44'/145'/0'" // default for BCH wallets
            visible: importAccount.isMnemonic
            color: Flowee.checkDerivation(text) ? palette.text : "red"
        }
    }
}
