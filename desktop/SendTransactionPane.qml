/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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

Rectangle {
    id: root
    function reset() {
        // reset fields
        bitcoinValueField.reset();
        bitcoinValueField.maxSelected = false;
        destination.text = "";
    }
    color: mainWindow.palette.window

    Column {
        x: 10
        y: 10
        spacing: 10
        width: parent.width - 20
        Label {
            text: qsTr("Destination") + ":"
            Layout.columnSpan: 2
        }
        RowLayout {
            width: parent.width
            TextField {
                id: destination
                focus: true
                property bool addressOk: addressType === Pay.CashPKH || (forceLegacyOk && addressType === Pay.LegacyPKH)
                property var addressType: Flowee.identifyString(text);
                property bool forceLegacyOk: false

                placeholderText: qsTr("Enter Bitcoin Cash Address")
                Layout.fillWidth: true
                Layout.columnSpan: 3
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
            Label {
                // TODO make button to override and set forceLegacyOk to true
                text: "!"
                visible: destination.addressType === Pay.LegacyPKH;
            }
        }

        GridLayout {
            columns: 3
            Label {
                text: qsTr("Amount %1").arg("EUR") + ":"
            }
            TextField {
                text: "0.0 EUR"
            }
            FloweeCheckBox {
                id: amountSelector
            }
            Label {
                id: payAmount
                text: qsTr("Amount") + ":"
            }
            BitcoinValueField {
                id: bitcoinValueField
                property bool maxSelected: false

                fontPtSize: payAmount.font.pointSize
                onValueChanged: maxSelected = false
            }
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
                        bitcoinValueField.value = portfolio.current.balanceConfirmed + portfolio.current.balanceUnconfirmed
                    } else {
                        bitcoinValueField.valueObject.strValue = previousAmountString
                    }

                    bitcoinValueField.maxSelected = isChecked
                }
            }
        }
        RowLayout {
            width: parent.width
            Layout.columnSpan: 3

            Item {
                width: 1; height: 1
                Layout.fillWidth: true
            }

            Button2 {
                id: nextButton
                text: qsTr("Sign")
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
                text: qsTr("Reset")
                onClicked: root.reset();
            }
        }
        FloweeGroupBox {
            id: txDetails
            Layout.columnSpan: 4
            title: "Transaction Details"
            isCollapsed: true
            width: parent.width
            // y: 10
            height:  Math.max(implicitHeight, button.height + 15)
            Button2 {
                id: button
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 10
                text: "Send"
            }

            content: Rectangle {
                width: 100
                height: 200
                color: "red"
            }
        }
    }

    /*
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
                text: qsTr("Destination") + ":"
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: checkAndSendTx.payment === null ? "waiting" : checkAndSendTx.payment.formattedTargetAddress
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
            Label {
                id: origText
                text: qsTr("Was", "user-entered-format") + ":"
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
                text: qsTr("Amount") + ":"
            }
            BitcoinAmountLabel {
                value: {
                    if (checkAndSendTx.payment === null)
                        return 0;
                    var val = checkAndSendTx.payment.paymentAmount;
                    if (val === -1)
                        return portfolio.current.balanceConfirmed + portfolio.current.balanceUnconfirmed;
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
                text: qsTr("More Details") + ":"
                font.italic: true
                Layout.columnSpan: 2
            }
            Label {
                // no need translating this one.
                text: "TxId:"
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
            }

            Label {
                text: checkAndSendTx.payment === null ? "" : checkAndSendTx.payment.txid
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Fee") + ":"
                Layout.alignment: Qt.AlignRight
            }

            Label {
                text: checkAndSendTx.payment === null ? "" : qsTr("%1 sats").arg(checkAndSendTx.payment.assignedFee)
            }
            Label {
                text: qsTr("Transaction size") + ":"
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: {
                    if (checkAndSendTx.payment === null)
                        return "";
                    var rc = checkAndSendTx.payment.txSize;
                    return qsTr("%1 bytes", "", rc).arg(rc)
                }
            }
            Label {
                text: qsTr("Fee per byte") + ":"
                Layout.alignment: Qt.AlignRight
            }
            Label {
                text: {
                    if (checkAndSendTx.payment === null)
                        return "";
                    var rc = checkAndSendTx.payment.assignedFee / checkAndSendTx.payment.txSize;

                    return qsTr("%1 sat/byte", "fee", rc).arg((rc).toFixed(3))
                }
            }
        }

        Button {
            id: button
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: checkAndSendTx.payment !== null && checkAndSendTx.payment.paymentOk
                  ? qsTr("Approve and Send", "button") : qsTr("Close", "button")
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
    */
}
