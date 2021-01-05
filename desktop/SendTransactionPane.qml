/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import QtQuick.Window 2.14

import Flowee.org.pay 1.0
import "./ControlColors.js" as ControlColors

Pane {
    id: root
    function reset() {
        // reset fields
        bitcoinValueField.reset();
        bitcoinValueField.maxSelected = false;
        destination.text = "";
    }
    opacity: visible ? 1 : 0
    GridLayout {
        id: grid
        width: parent.width - 20
        x: 10
        columns: 3

        Label {
            text: "Destination:"
        }
        TextField {
            id: destination
            focus: true
            property bool addressOk: {
                let res = Flowee.identifyString(text);
                return  res === Pay.CashPKH || res === Pay.LegacyPKH;
            }

            placeholderText: qsTr("Enter Bitcoin Address")
            Layout.fillWidth: true
            onFocusChanged: {
                if (activeFocus || text === "")
                    color = mainWindow.palette.text
                else if (!addressOk)
                    color = Flowee.useDarkSkin ? "#ff6568" : "red"
            }
        }
        Label {
            id: checked
            color: "green"
            font.pixelSize: 24
            text: destination.addressOk ? "✔" : " "
        }

        // next row
        Label {
            id: payAmount
            text: "Amount:"
        }
        BitcoinValueField {
            id: bitcoinValueField
            property bool maxSelected: false

            fontPtSize: payAmount.font.pointSize
            Layout.columnSpan: 2
            onValueChanged: maxSelected = false
        }

        RowLayout {
            Layout.columnSpan: 3

            Button2 {
                id: sendAll
                text: qsTr("Max")
                checkable: true
                checked: bitcoinValueField.maxSelected

                property string previousAmountString: ""
                onClicked: {
                    var isChecked = !bitcoinValueField.maxSelected // simply invert
                    if (isChecked) {
                        // backup what the user typed there, to be used if she no longer wants 'max'
                        previousAmountString = bitcoinValueField.valueObject.enteredString;
                        // the usage of 'account' here assumes we are under the hierarchy of the AccountPage
                        bitcoinValueField.value = account.balanceConfirmed + account.balanceUnconfirmed
                    } else {
                        bitcoinValueField.valueObject.strValue = previousAmountString
                    }

                    bitcoinValueField.maxSelected = isChecked
                }
            }

            Item {
                width: 1; height: 1
                Layout.fillWidth: true
            }

            Button2 {
                id: nextButton
                text: qsTr("Next")
                enabled: (bitcoinValueField.value > 0
                          || bitcoinValueField.maxSelected) && destination.addressOk;

                onClicked: {
                    if (bitcoinValueField.maxSelected)
                        var payment = portfolio.startPayAllToAddress(destination.text);
                    else
                        payment = portfolio.startPayToAddress(destination.text, bitcoinValueField.valueObject);

                    checkAndSendTx.payment = payment;
                    ControlColors.applySkin(checkAndSendTx);
                    payment.approveAndSign();
                }
            }
            Button2 {
                id: cancelbutton
                text: qsTr("Cancel")

                onClicked: {
                    root.visible = false;
                    root.reset();
                }
            }
        }

        /*
          TODO;
           - have a fiat value input.
        */
    }

    ApplicationWindow {
        id: checkAndSendTx
        visible: false
        title: qsTr("Validate and Send Transaction")
        modality: Qt.NonModal
        flags: Qt.Dialog
        width: header.implicitWidth + 20
        height: 20 + minimumHeight
        minimumHeight: {
            var h = header.implicitHeight + table.implicitHeight + button.height + 60;
            if (checkAndSendTx.payment !== null && checkAndSendTx.payment.paymentOk)
                h += table2.implicitHeight
            return h;
        }

        property QtObject payment: null

        onPaymentChanged: visible = payment !== null
        onVisibleChanged: {
            if (!visible) {
                if (payment !== null) {
                    console.log("user closed the window")
                    payment = null
                }
                else {
                    console.log("user accepted the payment")
                    root.visible = false;
                    root.reset();
                }
            }
        }

        Label {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            horizontalAlignment: Text.AlignHCenter
            text: checkAndSendTx.payment !== null && checkAndSendTx.payment.paymentOk ?
                      qsTr("Check your values and press approve to send payment.")
                    : qsTr("Not enough funds in account to make payment!");

            font.bold: true
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
        GridLayout {
            id: table
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            anchors.top: header.bottom
            columns: 2
            Label {
                text: qsTr("Destination:")
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: checkAndSendTx.payment === null ? "waiting" : checkAndSendTx.payment.formattedTargetAddress
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
            Label {
                id: origText
                text: qsTr("Was:")
                Layout.alignment: Qt.AlignRight
                visible: checkAndSendTx.payment !== null
                         && checkAndSendTx.payment.targetAddress !== checkAndSendTx.payment.formattedTargetAddress
            }
            Label {
                visible: origText.visible
                text: checkAndSendTx.payment === null ? "" : checkAndSendTx.payment.targetAddress
                wrapMode: Text.WrapAnywhere
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: "Value:"
            }
            BitcoinAmountLabel {
                value: {
                    if (checkAndSendTx.payment === null)
                        return 0;
                    var val = checkAndSendTx.payment.paymentAmount;
                    if (val === -1)
                        return account.balanceConfirmed + account.balanceUnconfirmed;
                    return val;
                }
                fontPtSize: origText.font.pointSize
                colorize: false
                textColor: mainWindow.palette.text
            }
        }
        GridLayout {
            id: table2
            visible: checkAndSendTx.payment !== null && checkAndSendTx.payment.paymentOk
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 20
            anchors.top: table.bottom
            columns: 2
            Label {
                text: qsTr("More Details:")
                font.italic: true
                Layout.columnSpan: 2
            }
            Label {
                text: qsTr("TxId:")
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
            }

            Label {
                text: checkAndSendTx.payment === null ? "" : checkAndSendTx.payment.txid
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Fee:")
                Layout.alignment: Qt.AlignRight
            }

            Label {
                text: checkAndSendTx.payment === null ? "" : qsTr("%1 sats").arg(checkAndSendTx.payment.assignedFee)
            }
            Label {
                text: qsTr("Transaction size:")
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: checkAndSendTx.payment === null ? "" : qsTr("%1 bytes").arg(checkAndSendTx.payment.txSize)
            }
            Label {
                text: qsTr("Fee per byte:")
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: checkAndSendTx.payment === null
                        ? "" : qsTr("%1 sat/byte")
                                .arg((checkAndSendTx.payment.assignedFee / checkAndSendTx.payment.txSize).toFixed(3))
            }
        }

        Button {
            id: button
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: checkAndSendTx.payment !== null && checkAndSendTx.payment.paymentOk
                  ? qsTr("Approve and Send") : qsTr("Close")
            onClicked: {
                if (checkAndSendTx.payment.paymentOk) {
                    checkAndSendTx.payment.sendTx();
                    checkAndSendTx.payment = null
                } else {
                    checkAndSendTx.close();
                }
            }
        }
    }

    Behavior on opacity { NumberAnimation { duration: 350 } }
}
