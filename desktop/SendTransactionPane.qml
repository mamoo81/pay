import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import Flowee.org.pay 1.0

FocusScope {
    id: root
    
    property bool enabled: false
    
    function start() {
        enabled = true;
        // var obj = wallets.startPayToAddress("qpgn5ka4jptc98a9ftycvujxx33e79nxuqlz5mvxns", 15000000);
        // obj.approveAndSend();

        // init fields
        bitcoinValueField.reset()

        root.focus = true
        destination.focus = true
    }

    function validate() {
        let res = Flowee.identifyString(destination.text);
        status.text = "validate " + res
        if (res === Pay.CashPKH || res === Pay.LegacyPKH) {
            // start next step
            status.text = "OK!"
        }
    }

    GridLayout {
        columns: 2
        anchors.fill: parent
        anchors.margins: 10

        Text {
            text: "Destination:"
        }
        TextField {
            id: destination

            placeholderText: "Search or enter bitcoin address"
            Layout.fillWidth: true
            onTextChanged: root.validate()
        }

        Text {
            id: payAmount
            text: "Amount:"
        }
        BitcoinValueField {
            id: bitcoinValueField
        }

        Pane { Layout.fillHeight: true }

        /*
          have a field to show the value. With BCH and in 'fiat'.
          have a swap button to switch beteen typing fiat or bch.

          Have an "Use all Available Funds (15 EUR)" button

          Have a "next" button
        */

        Text {
            id: status
            text: "waiting"
        }
    }
}
