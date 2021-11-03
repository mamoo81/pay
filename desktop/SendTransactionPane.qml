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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11
import "widgets" as Flowee
import "./ControlColors.js" as ControlColors
import Flowee.org.pay 1.0

Rectangle {
    id: root
    function reset() {
        // reset fields
        bitcoinValueField.reset();
        bitcoinValueField.maxSelected = false;
        destination.forceLegacyOk = false;
        destination.text = "";
        destination.updateColor();
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
            Flowee.TextField {
                id: destination
                focus: true
                property bool addressOk: (addressType === Bitcoin.CashPKH || addressType === Bitcoin.CashSH)
                                         || (forceLegacyOk && (addressType === Bitcoin.LegacySH || addressType === Bitcoin.LegacyPKH))
                property var addressType: Pay.identifyString(text);
                property bool forceLegacyOk: false

                placeholderText: qsTr("Enter Bitcoin Cash Address")
                Layout.fillWidth: true
                Layout.columnSpan: 3

                onFocusChanged: updateColor();
                onAddressOkChanged: updateColor()

                function updateColor() {
                    if (!activeFocus && text !== "" && !addressOk)
                        color = Pay.useDarkSkin ? "#ff6568" : "red"
                    else
                        color = mainWindow.palette.text
                }

            }
            Label {
                id: checked
                color: "green"

                font.pixelSize: 24
                text: destination.addressOk ? "✔" : " "
            }
        }

        Label {
            id: payAmount
            text: qsTr("Amount") + ":"
        }
        RowLayout {
            Flowee.FiatValueField {
                id: fiatValueField
                visible: Fiat.price > 0
                onValueEdited: {
                    amountSelector.checked = false;
                    bitcoinValueField.maxSelected = false;
                    bitcoinValueField.valueObject.enteredString = value / Fiat.price
                }

                function copyFromBCH(bchAmount) {
                    valueObject.enteredString = ((bchAmount / 100000000 * Fiat.price) + 0.5) / 100
                }
            }
            Flowee.CheckBox {
                id: amountSelector
                sliderOnIndicator: false
                visible: Fiat.price > 0
                enabled: false
            }
            Flowee.BitcoinValueField {
                id: bitcoinValueField
                property bool maxSelected: false
                property string previousAmountString: ""
                onValueEdited: {
                    maxSelected = false;
                    amountSelector.checked = true;
                    fiatValueField.copyFromBCH(value);
                }

                function update(setToMax) {
                    if (setToMax) {
                        // backup what the user typed there, to be used if she no longer wants 'max'
                        previousAmountString =  bitcoinValueField.valueObject.enteredString;
                        value = portfolio.current.balanceConfirmed + portfolio.current.balanceUnconfirmed
                    } else {
                        valueObject.enteredString = previousAmountString
                    }
                    fiatValueField.copyFromBCH(value);
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
            Flowee.Button {
                id: sendAll
                text: qsTr("Max")
                checkable: true
                checked: bitcoinValueField.maxSelected

                onClicked: {
                    var isChecked = !bitcoinValueField.maxSelected // simply invert
                    bitcoinValueField.update(isChecked);
                    bitcoinValueField.maxSelected = isChecked
                    if (isChecked)
                        amountSelector.checked = true
                }
            }
        }

        RowLayout {
            width: parent.width
            /* TODO future feature.
            Flowee.Button {
                text: qsTr("Add Advanced Option...")
            }
            */

            Item {
                width: 1; height: 1
                Layout.fillWidth: true
            }

            Flowee.Button {
                id: prepareButton
                text: qsTr("Prepare")
                enabled: (bitcoinValueField.value > 0
                          || bitcoinValueField.maxSelected) && destination.addressOk;

                property QtObject portfolioUsed: null

                onClicked: {
                    if (bitcoinValueField.maxSelected)
                        var payment = portfolio.startPayAllToAddress(destination.text);
                    else
                        payment = portfolio.startPayToAddress(destination.text, bitcoinValueField.valueObject);
                    delete root.payment;
                    root.payment = payment;
                    payment.approveAndSign();
                    portfolioUsed = portfolio.current
                }
            }
        }
        Label {
            text: qsTr("Not enough funds in account to make payment!")
            visible: root.payment != null && !root.payment.paymentOk
            color: txid.color // make sure this is 'disabled' when the warning is not for this wallet.
        }

        Flowee.GroupBox {
            id: txDetails
            Layout.columnSpan: 4
            title: qsTr("Transaction Details")
            width: parent.width

            GridLayout {
                columns: 2
                property bool txOk: root.payment != null && root.payment.paymentOk

                Label {
                    text: qsTr("Destination") + ":"
                    Layout.alignment: Qt.AlignRight
                    visible: finalDestination.visible
                }
                Label {
                    id: finalDestination
                    text: root.payment == null ? "" : root.payment.formattedTargetAddress
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                    visible: text !== "" && text != destination.text
                }
                Label {
                    // no need translating this one.
                    text: "TxId:"
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop
                }

                LabelWithClipboard {
                    id: txid
                    text: root.payment == null ? qsTr("Not prepared yet") : root.payment.txid
                    Layout.fillWidth: true
                    // Change the color when the portfolio changed since 'prepare' was clicked.
                    color: root.payment == null || prepareButton.portfolioUsed === portfolio.current
                            ? palette.text
                            : Qt.darker(palette.text, (Pay.useDarkSkin ? 1.6 : 0.4))
                }
                Label {
                    text: qsTr("Fee") + ":"
                    Layout.alignment: Qt.AlignRight
                }

                Flowee.BitcoinAmountLabel {
                    value: !parent.txOk ? 0 : root.payment.assignedFee
                    colorize: false
                    color: txid.color
                }
                Label {
                    text: qsTr("Transaction size") + ":"
                    Layout.alignment: Qt.AlignRight
                }
                Label {
                    text: {
                        if (!parent.txOk)
                            return "";
                        var rc = root.payment.txSize;
                        return qsTr("%1 bytes", "", rc).arg(rc)
                    }
                    color: txid.color
                }
                Label {
                    text: qsTr("Fee per byte") + ":"
                    Layout.alignment: Qt.AlignRight
                }
                Label {
                    text: {
                        if (!parent.txOk)
                            return "";
                        var rc = root.payment.assignedFee / root.payment.txSize;

                        var fee = rc.toFixed(3); // no more than 3 numbers behind the separator
                        fee = (fee * 1.0).toString(); // remove trailing zero's (1.000 => 1)
                        return qsTr("%1 sat/byte", "fee", rc).arg(fee);
                    }
                    color: txid.color
                }
            }
        }

        RowLayout {
            width: parent.width

            Item {
                width: 1; height: 1
                Layout.fillWidth: true
            }

            Flowee.Button {
                id: button
                text: qsTr("Send")
                enabled: root.payment != null && root.payment.paymentOk
                            && prepareButton.portfolioUsed === portfolio.current // also make sure we prepared for the current portfolio.
                onClicked: {
                    root.payment.sendTx();
                    root.payment = null
                    reset();
                }
            }
            Flowee.Button {
                text: qsTr("Cancel")
                onClicked: root.reset();
            }
        }
    }
    Item {
        // overlay item
        anchors.fill: parent
        Item {
            // BTC address warning.
            visible: (destination.addressType === Pay.LegacySH || destination.addressType === Pay.LegacyPKH)
                     && destination.forceLegacyOk === false;
            onVisibleChanged: {
                var pos = parent.mapFromItem(destination, 0, destination.height);
                // console.log("xxxx " + pos.x + ", " + pos.y);
                x = pos.x + 20
                y = pos.y + 25
            }

            width: destination.width - 40
            height: warningColumn.height + 20
            Rectangle {
                anchors.fill: warningColumn
                anchors.margins: -10
                color: warning.palette.window
                border.width: 3
                border.color: "red"
                radius: 10
            }
            Flowee.ArrowPoint {
                x: 40
                anchors.bottom: warningColumn.top
                anchors.bottomMargin: 5
                rotation: -90
                color: "red"
            }

            Column {
                id: warningColumn
                x: 10
                width: parent.width - 20
                spacing: 10
                Label {
                    font.bold: true
                    font.pixelSize: warning.font.pixelSize * 1.2
                    text: qsTr("Warning")
                }
                Label {
                    id: warning
                    width: parent.width
                    text: qsTr("This is a request to pay to a BTC address, which is an incompatible coin. Your funds could get lost and Flowee will have no way to recover them. Are you sure you want to pay to this BTC address?")
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                RowLayout {

                    width: parent.width
                    Item {
                        width: 1; height: 1
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Yes, I am sure")
                        onClicked: destination.forceLegacyOk = true
                    }
                    Button {
                        text: qsTr("No")
                        onClicked: {
                            destination.text = ""
                            destination.updateColor()
                        }
                    }
                }
            }
        }
    }
}
