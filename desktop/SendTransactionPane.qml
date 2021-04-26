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
        delete root.payment;
        root.payment = null;
    }
    color: mainWindow.palette.window

    property QtObject payment: null

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

        Label {
            id: payAmount
            text: qsTr("Amount") + ":"
        }
        RowLayout {
            /* TODO hook this up when we add fiat support.
            TextField {
                text: "0.0 EUR"
            }
            FloweeCheckBox {
                id: amountSelector
            }
            */
            BitcoinValueField {
                id: bitcoinValueField
                property bool maxSelected: false
                property string previousAmountString: ""

                fontPtSize: payAmount.font.pointSize
                onValueChanged: maxSelected = false

                function update(setToMax) {
                    if (setToMax) {
                        // backup what the user typed there, to be used if she no longer wants 'max'
                        previousAmountString = bitcoinValueField.valueObject.enteredString;
                        value = portfolio.current.balanceConfirmed + portfolio.current.balanceUnconfirmed
                    } else {
                        valueObject.strValue = previousAmountString
                    }
                }
                Connections {
                    target: portfolio
                    function onCurrentChanged() {
                        var setToMax = bitcoinValueField.maxSelected
                        if (setToMax) {
                            bitcoinValueField.update(setToMax);
                            bitcoinValueField.maxSelected = setToMax; // undo any changes in the button
                        }
                    }
                }
            }
            Button2 {
                id: sendAll
                text: qsTr("Max")
                checkable: true
                checked: bitcoinValueField.maxSelected

                onClicked: {
                    var isChecked = !bitcoinValueField.maxSelected // simply invert
                    bitcoinValueField.update(isChecked);
                    bitcoinValueField.maxSelected = isChecked
                }
            }

        }

        RowLayout {
            width: parent.width
            /* TODO future feature.
            Button2 {
                text: qsTr("Add Advanced Option...")
            }
            */

            Item {
                width: 1; height: 1
                Layout.fillWidth: true
            }

            Button2 {
                text: qsTr("Prepare")
                enabled: (bitcoinValueField.value > 0
                          || bitcoinValueField.maxSelected) && destination.addressOk;

                onClicked: {
                    if (bitcoinValueField.maxSelected)
                        var payment = portfolio.startPayAllToAddress(destination.text);
                    else
                        payment = portfolio.startPayToAddress(destination.text, bitcoinValueField.valueObject);
                    delete root.payment;
                    root.payment = payment;
                    payment.approveAndSign();
                }
            }
        }
        FloweeGroupBox {
            id: txDetails
            Layout.columnSpan: 4
            title: qsTr("Transaction Details")
            width: parent.width

            content: GridLayout {
                columns: 2

            Label {
                text: qsTr("Destination") + ":"
                Layout.alignment: Qt.AlignRight
                visible: finalDestination.visible
            }
            Label {
                id: finalDestination
                text: root.payment === null ? "" : root.payment.formattedTargetAddress
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
                visible: text !== "" && text != destination.text
            }
            Label {
                // no need translating this one.
                text: "TxId:"
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
            }

            Label {
                text: root.payment === null ? qsTr("Not prepared yet") : root.payment.txid
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
                Label {
                    text: qsTr("Fee") + ":"
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: root.payment === null ? "" : qsTr("%1 sats").arg(root.payment.assignedFee)
                    // change the Fee to be displayed in current unit.
                }
                Label {
                    text: qsTr("Transaction size") + ":"
                    Layout.alignment: Qt.AlignRight
                }
                Label {
                    text: {
                        if (root.payment === null)
                            return "";
                        var rc = root.payment.txSize;
                        return qsTr("%1 bytes", "", rc).arg(rc)
                    }
                }
                Label {
                    text: qsTr("Fee per byte") + ":"
                    Layout.alignment: Qt.AlignRight
                }
                Label {
                    text: {
                        if (root.payment === null)
                            return "";
                        var rc = root.payment.assignedFee / root.payment.txSize;

                        var fee = rc.toFixed(3); // no more than 3 numbers behind the separator
                        fee = (fee * 1.0).toString(); // remove trailing zero's (1.000 => 1)
                        return qsTr("%1 sat/byte", "fee", rc).arg(fee);
                    }
                }
            }
        }

        RowLayout {
            width: parent.width

            Item {
                width: 1; height: 1
                Layout.fillWidth: true
            }

            Button2 {
                id: button
                text: qsTr("Send")
                enabled: root.payment !== null && root.payment.paymentOk;
                onClicked: {
                    root.payment.sendTx();
                    root.payment = null
                    reset();
                }
            }
            Button2 {
                text: qsTr("Cancel")
                onClicked: root.reset();
            }
        }

    }
}
