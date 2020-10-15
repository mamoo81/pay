/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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
        bitcoinValueField.reset()
        destination.text = ""
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

            placeholderText: qsTr("Enter bitcoin address")
            Layout.fillWidth: true
            text: activeFocus ? "I have active focus!" : "I do not have active focus"
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
            Layout.columnSpan: 2
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.columnSpan: 3

            Button2 {
                id: nextButton
                text: qsTr("Next")
                enabled: bitcoinValueField.value > 0 && destination.addressOk;

                onClicked: {
                    checkAndSendTx.payment
                            = wallets.startPayToAddress(destination.text, bitcoinValueField.valueObject);
                    ControlColors.applySkin(checkAndSendTx);
                    checkAndSendTx.payment.approveAndSign();
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

           - Have an "Use all Available Funds (15 EUR)" button
        */
    }

    ApplicationWindow {
        id: checkAndSendTx
        visible: false
        title: qsTr("Validate and Send Transaction")
        modality: Qt.NonModal
        flags: Qt.Dialog
        width: header.implicitWidth + 20
        height: 100 + minimumHeight
        minimumHeight:  header.implicitHeight + table.implicitHeight + table2.implicitHeight + button.height + 60

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

            text: qsTr("Check your values and press approve to send payment.")
            font.bold: true
            color: "white"
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
                value: checkAndSendTx.payment === null ? 0 : checkAndSendTx.payment.paymentAmount
                fontPtSize: origText.font.pointSize
                colorize: false
                textColor: mainWindow.palette.text
            }
        }
        GridLayout {
            id: table2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            anchors.top: table.bottom
            columns: 2
            Label {
                text: qsTr("More Details")
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
                text: qsTr("Fee (total):")
                Layout.alignment: Qt.AlignRight
            }

            Label {
                text: checkAndSendTx.payment === null ? 0 : checkAndSendTx.payment.assignedFee
            }
            Label {
                text: qsTr("transaction size:")
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: checkAndSendTx.payment === null ? 0 : checkAndSendTx.payment.txSize
            }
            Label {
                text: qsTr("Fee (per byte):")
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: checkAndSendTx.payment === null ? 0
                                                      : (checkAndSendTx.payment.assignedFee / checkAndSendTx.payment.txSize).toFixed(3);
            }
        }

        Button {
            id: button
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: qsTr("Approve and Send")
            onClicked:  {
                checkAndSendTx.payment.sendTx();
                checkAndSendTx.payment = null
            }
        }
    }

    Behavior on opacity { NumberAnimation { duration: 350 } }
}
