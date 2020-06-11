import QtQuick 2.14
// import QtQml.StateMachine 1.0 as DSM
// import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
// import QtQuick.Dialogs 1.3
import Flowee.org.pay 1.0

Item {
    id: root
    
    // signal startedTransaction
    property bool enabled: false
    
    function start() {
        enabled = true;
        // startedTransaction(); // TODO, change the statemachine based on this.
        // TODO init fields
        
        // var obj = wallets.startPayToAddress("qpgn5ka4jptc98a9ftycvujxx33e79nxuqlz5mvxns", 15000000);
        // obj.feePerByte = 1;
        // obj.approveAndSend();
        console.log("go!");
    }

    function validate() {
        let res = Flowee.identifyString(destination.text);
        console.log("validate " + res)
        if (res === Flowee.CashPKH || res === Flowee.LegacyPKH) {
            // start next step
            console.log("Ok!")

        }
    }

    GridLayout {
        columns: 2
        anchors.fill: parent

        TextField {
            id: destination

            placeholderText: "Search or enter bitcoin address"
            Layout.fillWidth: true
            Layout.columnSpan: 2
            onTextChanged: root.validate()
        }

        /*
          have a field to show the value. With BCH and in 'fiat'.
          have a swap button to switch beteen typing fiat or bch.

          Have an "Use all Available Funds (15 EUR)" button

          Have an "backspace" button and all digits.

          Have a "next" button
        */
    }
}
