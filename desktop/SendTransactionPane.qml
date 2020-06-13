import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import QtQuick.Dialogs 1.3
import Flowee.org.pay 1.0

FocusScope {
    id: root

    property bool enabled: false
    
    function start() {
        enabled = true;
        // init fields
        bitcoinValueField.reset()
        destination.text = ""

        root.focus = true
        destination.focus = true
    }

    GridLayout {
        columns: 3
        anchors.fill: parent
        anchors.margins: 10

        Text {
            text: "Destination:"
        }
        TextField {
            id: destination
            property bool addressOk: {
                let res = Flowee.identifyString(text);
                return  res === Pay.CashPKH || res === Pay.LegacyPKH;
            }

            placeholderText: "Search or enter bitcoin address"
            Layout.fillWidth: true

            onFocusChanged: {
                if (activeFocus)
                    color = "black"
                else if (!addressOk)
                    color = "red"
            }
        }
        Text {
            id: checked
            color: "green"
            font.pixelSize: 24
            text: destination.addressOk ? "✔" : " "
        }

        // next row
        Text {
            id: payAmount
            text: "Amount:"
        }
        BitcoinValueField {
            id: bitcoinValueField
            Layout.columnSpan: 2
        }

        Pane {
            Layout.fillHeight: true;
            Layout.fillWidth: true;
            Layout.columnSpan: 3

            Button {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 20
                id: nextButton
                text: qsTr("Next")
                enabled: bitcoinValueField.value > 0 && destination.addressOk;

                onClicked: {
                    checkAndSendTx.payment
                            = wallets.startPayToAddress(destination.text, bitcoinValueField.valueObject);

                    checkAndSendTx.payment.approveAndSign()
                }
            }
        }

        /*
          TODO;
           - have a fiat value input.

           - Have an "Use all Available Funds (15 EUR)" button
        */
    }

    AbstractDialog {
        id: checkAndSendTx
        visible: payment !== null
        title: "Validate and Send Transaction"

        property QtObject payment: null

        onVisibleChanged: {
            if (!visible) {
                payment = null
                root.enabled = false
            }
        }
        Pane {
            anchors.fill: parent
            anchors.margins: 6

            GridLayout {
                id: diag
                columns: 2
                anchors.fill: parent

                Text {
                    text: qsTr("Check your values and press approve to send payment.")
                    font.bold: true
                    Layout.columnSpan: 2
                }

                Text {
                    text: "Destination:"
                }
                Text {
                    text: checkAndSendTx.payment === null ? "waiting" : checkAndSendTx.payment.formattedTargetAddress
                }
                Text {
                    text: "Value:"
                }
                BitcoinAmountLabel {
                    value: checkAndSendTx.payment === null ? 0 : checkAndSendTx.payment.paymentAmount
                    colorize: false
                }

                Text {
                    text: "TxId:"
                }

                Text {
                    text: checkAndSendTx.payment === null ? "waiting" : checkAndSendTx.payment.txid
                    wrapMode: Text.WrapAnywhere
                }

                Text {
                    text: "Fee (total):"
                }

                Text {
                    text: checkAndSendTx.payment === null ? 0 : checkAndSendTx.payment.assignedFee
                }
                Text {
                    text: "transaction size:"
                }
                Text {
                    text: checkAndSendTx.payment === null ? 0 : checkAndSendTx.payment.txSize
                }
                Text {
                    text: "Fee (per byte):"
                }

                Text {
                    text: checkAndSendTx.payment === null ? 0
                                  : (checkAndSendTx.payment.assignedFee / checkAndSendTx.payment.txSize);
                }

                Pane {
                    implicitHeight: button.height + 50
                    Button {
                        id: button
                        text: qsTr("Approve and Send")
                        onClicked:  {
                            checkAndSendTx.payment.sendTx();
                            checkAndSendTx.payment = null
                        }
                        anchors.centerIn: parent
                    }
                    Layout.columnSpan: 2
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
            }
        }
    }
}
