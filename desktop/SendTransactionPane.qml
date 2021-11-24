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

Item {
    id: sendPanel

    Payment { // the model behind the Payment logic
        id: payment
        fiatPrice: Fiat.price
        account: portfolio.current
    }
    Rectangle { // background
        anchors.fill: parent
        color: mainWindow.palette.window
    }
    Flickable {
        id: contentArea
        width: sendPanel.width - 20
        y: 40
        x: 10

        height: parent.height - 40
        contentHeight: mainColumn.height
        contentWidth: width
        clip: true
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
                            if (modelData.type === Payment.InputSelector)
                                return inputFields
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
                Flowee.Button {
                    text: qsTr("Add Destination")
                    onClicked: payment.addExtraOutput();
                }
                Item { Layout.fillWidth: true }
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
                text: payment.error
                visible: text !== ""
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
    }

    // the panel that allows us to tweak the payment
    PaymentTweakingPanel {
        anchors.fill: parent
    }

    // ============= Payment components  ===============

    Component {
        id: destinationFields
        Flowee.GroupBox {
            id: destinationPane
            property QtObject paymentDetail: null

            collapsable: paymentDetail.collapsable
            onCollapsedChanged: paymentDetail.collapsed = collapsed
            collapsed: paymentDetail.collapsed
            title: qsTr("Destination")
            summary: {
                var ad = paymentDetail.address
                if (ad === "")
                    ad = "\'\'";
                if (paymentDetail.fiatFollows) {
                    if (paymentDetail.maxSelected)
                        var amount = qsTr("Max available", "The maximum balance available")
                    else
                        amount = Pay.priceToStringPretty(paymentDetail.paymentAmount)
                                + " " + Pay.unitName;
                }
                else {
                    amount = Fiat.formattedPrice(paymentDetail.fiatAmount)
                }

                return qsTr("%1 to %2", "summary text to pay X-euro to address M")
                            .arg(amount).arg(ad);
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

                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                    onActiveFocusChanged: updateColor();
                    onAddressOkChanged: updateColor()

                    placeholderText: qsTr("Enter Bitcoin Cash Address")
                    text: destinationPane.paymentDetail.address
                    onTextChanged: {
                        destinationPane.paymentDetail.address = text
                        updateColor();
                    }

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
                    onValueEdited: destinationPane.paymentDetail.fiatAmount = value
                    value: destinationPane.paymentDetail.fiatAmount
                }
                Flowee.CheckBox {
                    id: amountSelector
                    sliderOnIndicator: false
                    visible: Fiat.price > 0
                    enabled: false
                    checked: destinationPane.paymentDetail.fiatFollows
                }
                Flowee.BitcoinValueField {
                    id: bitcoinValueField
                    value: destinationPane.paymentDetail.paymentAmount
                    onValueEdited: destinationPane.paymentDetail.paymentAmount = value
                }
                Flowee.Button {
                    id: sendAll
                    visible: destinationPane.paymentDetail.maxAllowed
                    text: qsTr("Max")
                    checkable: true
                    checked: destinationPane.paymentDetail.maxSelected
                    onClicked: destinationPane.paymentDetail.maxSelected = checked
                }
            }
        }
    }


    /*
     * The input selector component.
     */
    Component {
        id: inputFields
        Flowee.GroupBox {
            id: inputsPane
            collapsable: paymentDetail.collapsable
            collapsed: paymentDetail.collapsed
            onCollapsedChanged: paymentDetail.collapsed = collapsed
            property QtObject paymentDetail: null
            title: qsTr("Input Selector")
            summary: qsTr("Selected %1 %2 in %3 coins", "selected 2 BCH in 5 coins", paymentDetail.selectedCount)
                            .arg(Pay.priceToStringPretty(paymentDetail.selectedValue))
                            .arg(Pay.unitName)
                            .arg(paymentDetail.selectedCount)

            columns: 4
            Label {
                text: qsTr("Total", "Number of coins") + ":"
            }
            Label {
                text: coinsListView.count
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Needed") +":"
            }
            Flowee.BitcoinAmountLabel {
                id: neededAmountLabel
                value: payment.paymentAmount
                Layout.fillWidth: true
                colorize: false
            }
            // next row
            Label {
                text: qsTr("Selected") + ":"
            }
            Label {
                text: inputsPane.paymentDetail.selectedCount
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Value") + ":"
            }
            Flowee.BitcoinAmountLabel {
                value: inputsPane.paymentDetail.selectedValue
                Layout.fillWidth: true
                colorize: false
            }

            // next row
            ListView {
                id: coinsListView
                clip: true
                Layout.columnSpan: 4
                Layout.fillWidth: true
                implicitHeight: {
                    var ch = contentHeight
                    var suggested = contentArea.height * 0.8
                    if (ch < 0 || suggested < ch)
                        return suggested
                    return ch
                }
                model: inputsPane.paymentDetail.coins

                property bool menuIsOpen: false

                delegate: Rectangle {
                    width: ListView.view.width - 5
                    height: mainText.height + ageLabel.height + 12
                    color: index %2 == 0 ? mainText.palette.alternateBase : mainText.palette.base

                    Rectangle {
                        id: lockedRect
                        color: Pay.useDarkSkin ? "#002558" : "#1a6ae2"
                        anchors.fill: parent
                        visible: locked // if the UTXO is user-locked

                        ToolTip {
                            delay: 600
                            text: qsTr("Locked coins will never be used for payments. Right-click for menu.")
                            visible: rowMouseArea.containsMouse
                        }
                    }

                    CheckBox {
                        y: 6
                        id: selectedBox
                        checked: model.selected
                        visible: !lockedRect.visible
                    }
                    Label {
                        id: mainText
                        y: 6
                        anchors.right: amountLabel.left
                        anchors.left: parent.left
                        anchors.leftMargin: 50
                        text: {
                            var fancy = cloakedAddress;
                            if (typeof fancy != "undefined")
                                return fancy;
                            return address;
                        }

                        elide: Label.ElideRight
                    }
                    Flowee.BitcoinAmountLabel {
                        id: amountLabel
                        value: model.value
                        anchors.baseline: mainText.baseline
                        anchors.right: parent.right
                        anchors.rightMargin: 30
                    }
                    Label {
                        id: ageLabel
                        text: qsTr("Age") + ": " + age
                        anchors.left: mainText.left
                        anchors.top: mainText.bottom
                        font.pixelSize: mainText.font.pixelSize * 0.8
                    }
                    MouseArea {
                        id: rowMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: locked

                        onClicked: {
                            // make it easy for the user to close a menu with either mouse
                            // button without instantly triggering another action.
                            if (coinsListView.menuIsOpen) {
                                coinsListView.menuIsOpen = false
                                return;
                            }
                            if (mouse.button == Qt.LeftButton) {
                                var willCheck = !selectedBox.checked
                                selectedBox.checked = willCheck
                                inputsPane.paymentDetail.setRowIncluded(index, willCheck)
                            }
                            else {
                                coinsListView.menuIsOpen = true
                                // Make sure that the menu
                                // opens where we clicked.
                                mousePos.x = mouse.x
                                mousePos.y = mouse.y
                                lockingMenu.open();
                            }
                        }
                        Item {
                            id: mousePos
                            width: 1; height: 1
                            Menu {
                                id: lockingMenu
                                MenuItem {
                                    text: selectedBox.checked ? qsTr("Unselect All") : qsTr("Select All")
                                    onClicked: {
                                        coinsListView.menuIsOpen = false
                                        if (selectedBox.checked)
                                            inputsPane.paymentDetail.unselectAll();
                                        else
                                            inputsPane.paymentDetail.selectAll();
                                    }
                                }
                                MenuItem {
                                    text: locked ? qsTr("Unlock coin") : qsTr("Lock coin")
                                    onClicked: {
                                        inputsPane.paymentDetail.setOutputLocked(index, !locked)
                                        coinsListView.menuIsOpen = false
                                    }
                                }
                            }
                        }
                    }
                    Image {
                        id: fusedIcon
                        visible: fusedCount > 0
                        source: "qrc:/cashfusion.svg"
                        anchors.right: parent.right
                        anchors.verticalCenter: mainText.verticalCenter
                        width: 24
                        height: 24
                        ToolTip {
                            delay: 200
                            text: qsTr("Coin has been fused for increased anonymity")
                            visible: mouseArea.containsMouse
                        }
                        MouseArea {
                            id: mouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            anchors.margins: -5
                        }
                    }
                }
            }
        }
    }
}
