import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import Flowee.org.pay 1.0

ColumnLayout {
    id: newAccountCreateHDAccount
    spacing: 10
    Label {
        id: title
        text: qsTr("This creates a new empty wallet with smart creation of addresses from a single seed")
        width: parent.width
    }
    RowLayout {
        id: nameRow
        Label {
            text: qsTr("Name:");
            Layout.alignment: Qt.AlignBaseline
        }
        FloweeTextField {
            id: accountName
        }
    }
    Item {
        height: button.height
        Layout.fillWidth: true

        Button2 {
            id: button
            text: qsTr("Go")
            anchors.top: nameRow.bottom
            anchors.right: parent.right
            onClicked: {
                var options = Flowee.createNewWallet(derivationPath.text, /* password */"", accountName.text);
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
        Layout.fillWidth: true
        isCollapsed: true
        columns: 2

        /*
        FloweeCheckBox {
            id: schnorr
            text: qsTr("Default to signing using ECDSA");
            tooltipText: qsTr("When enabled, newer style Schnorr signatures are not set as default for this wallet.")
            Layout.columnSpan: 2
        } */
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
