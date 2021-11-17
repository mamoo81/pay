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

Pane {
    id: root
    Payment { // the model behind the Payment logic
        id: payment
    }

    Column {
        id: mainColumn
        width: parent.width
        spacing: 10

        Repeater {
            model: payment.details
            delegate: Item {
                width: mainColumn.width
                height: loader.height + 6

                Loader {
                    id: loader
                    width: parent.width
                    height: status === Loader.Ready ? item.implicitHeight : 0
                    sourceComponent: {
                        if (modelData.type === Payment.PayToAddress)
                            return destinationFields
                        return null; // should never happen
                    }
                    onLoaded: item.paymentDetail = modelData
                }

                Rectangle {
                    id: deleteDetailButton
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    y: -3
                    width: 32
                    height: 32
                    visible: modelData.collapsable && !modelData.collapsed
                    color: mouseArea.containsMouse ? mainWindow.palette.button : mainWindow.palette.window
                    border.color: mainWindow.palette.button

                    Image {
                        source: "qrc:/edit-delete.svg"
                        width: 24
                        height: 24
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        anchors.margins: -3
                        cursorShape: Qt.ArrowCursor
                        hoverEnabled: true
                        onClicked: okCancelDiag.visible = true;
                    }
                }

                Flowee.Dialog {
                    id: okCancelDiag
                    onAccepted: payment.remove(modelData);
                    title: "Confirm delete"
                    text: "Do you really want to delete this detail?"
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: 0
            ToolButton {
                text: qsTr("Add…")
                onClicked: addDetailMenu.open(spacer);
            }
            Item {
                id: spacer
                width: 1
                height: prepareButton.height
                Layout.fillWidth: true
                Menu {
                    id: addDetailMenu
                    MenuItem {
                        text: "Destination"
                        onTriggered: payment.addExtraOutput();
                    }
                }
            }

            Flowee.Button {
                id: prepareButton
                text: qsTr("Prepare")
                enabled: payment.isValid

                property QtObject portfolioUsed: null

                onClicked: {
                    portfolioUsed = portfolio.current
                    payment.prepare(portfolioUsed);
                }
            }
        }
        Label {
            text: qsTr("Not enough funds in account to make payment!")
            visible: !payment.walletOk
            color: txid.color // make sure this is 'disabled' when the warning is not for this wallet.
        }

        Flowee.GroupBox {
            id: txDetails
            Layout.columnSpan: 4
            title: qsTr("Transaction Details")
            width: parent.width

            GridLayout {
                columns: 2
                property bool txOk: payment.txPrepared

                Repeater {
                    model: payment.details
                    delegate: RowLayout {
                        Layout.columnSpan: 2
                        visible: modelData.type === Payment.PayToAddress
                                    && modelData.formattedTarget !== ""
                                    && modelData.formattedTarget !== modelData.address
                        Label {
                            text: qsTr("Destination") + ":"
                            visible: finalDestination.visible
                        }
                        Label {
                            id: finalDestination
                            text: modelData.type === Payment.PayToAddress ? modelData.formattedTarget : ""
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }

                Label {
                    // no need translating this one.
                    text: "TxId:"
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop
                }

                LabelWithClipboard {
                    id: txid
                    text: payment.txid === "" ? qsTr("Not prepared yet") : payment.txid
                    Layout.fillWidth: true
                    // Change the color when the portfolio changed since 'prepare' was clicked.
                    color: prepareButton.portfolioUsed === portfolio.current
                            ? palette.text
                            : Qt.darker(palette.text, (Pay.useDarkSkin ? 1.6 : 0.4))
                }
                Label {
                    text: qsTr("Fee") + ":"
                    Layout.alignment: Qt.AlignRight
                }

                Flowee.BitcoinAmountLabel {
                    value: !parent.txOk ? 0 : payment.assignedFee
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
                        var rc = payment.txSize;
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
                        var rc = payment.assignedFee / payment.txSize;

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
                enabled: payment.txPrepared
                            && prepareButton.portfolioUsed === portfolio.current // also make sure we prepared for the current portfolio.
                onClicked: payment.broadcast();
            }
            Flowee.Button {
                text: qsTr("Cancel")
                onClicked: payment.reset();
            }
        }
    }

    // ============= Payment components  ===============

    Component {
        id: destinationFields
        Flowee.GroupBox {
            id: destinationPane
            property QtObject paymentDetail: null

            collapsable: paymentDetail.collapsable
            collapsed: paymentDetail.collapsed
            title: qsTr("Destination") + ":"
            columns: 1
            onCollapsedChanged: paymentDetail.collapsed = collapsed

            RowLayout {
                width: parent.width
                Flowee.TextField {
                    id: destination
                    focus: true
                    property bool addressOk: (addressType === Bitcoin.CashPKH || addressType === Bitcoin.CashSH)
                                             || (forceLegacyOk && (addressType === Bitcoin.LegacySH || addressType === Bitcoin.LegacyPKH))
                    property var addressType: Pay.identifyString(text);
                    property bool forceLegacyOk: false

                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                    onFocusChanged: updateColor();
                    onAddressOkChanged: updateColor()

                    placeholderText: qsTr("Enter Bitcoin Cash Address")
                    text: destinationPane.paymentDetail.address
                    onTextChanged: destinationPane.paymentDetail.address = text

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
                    property bool maxAllowed: destinationPane.paymentDetail.maxAllowed
                    property string previousAmountString: ""
                    onValueEdited: {
                        maxSelected = false;
                        amountSelector.checked = true;
                        fiatValueField.copyFromBCH(value);
                    }
                    // send back to model on change, model validates correctness
                    onValueChanged: {
                        console.log("Value changed " + maxAllowed + " " +  maxSelected + " "
                                    + value);
                        if (maxAllowed && maxSelected)
                            destinationPane.paymentDetail.paymentAmount = -1;
                        else
                            destinationPane.paymentDetail.paymentAmount = value;
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
                            var setToMax = bitcoinValueField.maxSelected && bitcoinValueField.maxAllowed
                            if (setToMax) {
                                bitcoinValueField.update(setToMax);
                                bitcoinValueField.maxSelected = setToMax; // undo any changes in the button
                            }
                        }
                    }
                }
                Flowee.Button {
                    id: sendAll
                    visible: bitcoinValueField.maxAllowed
                    text: qsTr("Max")
                    checkable: true
                    checked: bitcoinValueField.maxSelected

                    onClicked: {
                        var isChecked = !bitcoinValueField.maxSelected // simply invert
                        bitcoinValueField.maxSelected = isChecked
                        bitcoinValueField.update(isChecked);
                        if (isChecked)
                            amountSelector.checked = true
                    }
                }
            }
        }
    }
}
