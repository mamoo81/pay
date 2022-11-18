/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
import QtQuick.Shapes 1.11 // for shape-path
import "../Flowee" as Flowee
import "../ControlColors.js" as ControlColors
import Flowee.org.pay 1.0

Item {
    id: sendPanel
    focus: true

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
                        title: qsTr("Confirm delete")
                        text: qsTr("Do you really want to delete this detail?")
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
                        if (payment.walletNeedsPin) {
                            passwdDialog.start()
                        } else {
                            payment.prepare();
                        }
                    }
                }
                Flowee.PasswdDialog {
                    id: passwdDialog
                    title: qsTr("Enter your PIN")

                    onAccepted: {
                        payment.decrypt(pwd);
                        if (payment.error === "")
                            payment.prepare();
                    }
                }
            }
            Flowee.WarningLabel {
                id: warningLabel
                width: parent.width
                text: payment.error
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

                    Label {
                        // no need translating this one.
                        text: "TxId:"
                        Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    }
                    Flowee.LabelWithClipboard {
                        id: txid
                        text: payment.txid === "" ? qsTr("Not prepared yet") : payment.txid
                        Layout.fillWidth: true
                        // Change the color when the portfolio changed since 'prepare' was clicked.
                        color: prepareButton.portfolioUsed === portfolio.current
                                ? palette.text
                                : Qt.darker(palette.text, (Pay.useDarkSkin ? 1.6 : 0.4))
                        menuText: qsTr("Copy transaction-ID")
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
                            return qsTr("%1 bytes").arg(rc)
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
                            return qsTr("%1 sat/byte", "fee").arg(fee);
                        }
                        color: txid.color
                    }
                }
            }

            Flowee.DialogButtonBox {
                id: box
                width: parent.width
                standardButtons: DialogButtonBox.Cancel | DialogButtonBox.Ok
                onStandardButtonsChanged: {
                    var okButton = standardButton(DialogButtonBox.Ok)
                    if (okButton !== null) {
                        okButton.text = qsTr("Send")
                        okButton.enabled = canSend
                    }
                }
                property bool canSend: payment.isValid && payment.txPrepared
                                && prepareButton.portfolioUsed === portfolio.current; // also make sure we prepared for the current portfolio.
                onCanSendChanged: setEnabled(DialogButtonBox.Ok, canSend)
                onRejected: payment.reset();
                onAccepted: payment.broadcast();
            }
        }
    }

    // the panel that allows us to tweak the payment (add details)
    PaymentTweakingPanel {
        anchors.fill: parent
    }
    Keys.forwardTo: Flowee.ListViewKeyHandler {
        id: listViewKeyHandler
    }

    Control {
        id: broadcastFeedback
        anchors.fill: parent
        states: [
            State {
                name: "notStarted"
                when: payment.broadcastStatus === Payment.NotStarted
            },
            State {
                name: "preparing"
                when: payment.broadcastStatus === Payment.TxOffered
                PropertyChanges { target: background;
                    opacity: 1
                    y: 0
                }
                PropertyChanges { target: progressCircle; sweepAngle: 90 }
                StateChangeScript { script: ControlColors.applyLightSkin(broadcastFeedback) }
            },
            State {
                name: "sent1" // sent to only one peer
                extend: "preparing"
                when: payment.broadcastStatus === Payment.TxSent1
                PropertyChanges { target: progressCircle; sweepAngle: 150 }
            },
            State {
                name: "waiting" // waiting for possible rejection.
                when: payment.broadcastStatus === Payment.TxWaiting
                extend: "preparing"
                PropertyChanges { target: progressCircle; sweepAngle: 320 }
            },
            State {
                name: "success" // no reject, great success
                when: payment.broadcastStatus === Payment.TxBroadcastSuccess
                extend: "preparing"
                PropertyChanges { target: progressCircle
                    sweepAngle: 320
                    startAngle: -20
                }
                PropertyChanges { target: checkShape; opacity: 1 }
            },
            State {
                name: "rejected" // a peer didn't like our tx
                when: payment.broadcastStatus === Payment.TxRejected
                extend: "preparing"
                StateChangeScript { script: ControlColors.applyDarkSkin(broadcastFeedback) }
                PropertyChanges { target: background; color: "#7f0000" }
                PropertyChanges { target: circleShape; opacity: 0 }
                PropertyChanges {
                    target: txidFeedbackLabel
                    text: qsTr("Transaction rejected by network")
                }
            }
        ]
        Rectangle {
            id: background
            width: parent.width
            height: parent.height
            opacity: 0
            visible: opacity != 0
            color: mainWindow.floweeGreen
            y: height + 2

            Label {
                id: title
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 15
                y: 25
                text: qsTr("Payment Sent")
            }

            // The 'progress' icon.
            Shape {
                id: circleShape
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: title.bottom
                anchors.topMargin: 10
                width: 160
                height: width
                smooth: true
                ShapePath {
                    strokeWidth: 20
                    strokeColor: "#dedede"
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 100; startY: 10

                    PathAngleArc {
                        id: progressCircle
                        centerX: 80
                        centerY: 80
                        radiusX: 70; radiusY: 70
                        startAngle: -80
                        sweepAngle: 0
                        Behavior on sweepAngle {  NumberAnimation { duration: 2500 } }
                        Behavior on startAngle {  NumberAnimation { } }
                    }
                }
                Behavior on opacity {  NumberAnimation { } }
            }
            Shape {
                id: checkShape
                anchors.fill: circleShape
                smooth: true
                opacity: 0
                ShapePath {
                    id: checkPath
                    strokeWidth: 16
                    strokeColor: "green"
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 52; startY: 80
                    PathLine { x: 76; y: 110 }
                    PathLine { x: 125; y: 47 }
                }
            }

            Label {
                id: fiatAmount
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: circleShape.bottom
                anchors.topMargin: 10
                font.pixelSize: 24
                text: Fiat.formattedPrice(payment.effectiveFiatAmount)
                visible: Fiat.price !== 0
            }
            Flowee.BitcoinAmountLabel {
                id: cryptoAmount
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: fiatAmount.bottom
                anchors.topMargin: 20
                fontPixelSize: 28
                value: payment.effectiveBchAmount
                colorize: false
                showFiat: false
            }
            Flowee.LabelWithClipboard {
                id: txidFeedbackLabel
                anchors.top: cryptoAmount.bottom
                anchors.topMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                menuText: qsTr("Copy transaction-ID")
                clipboardText: payment.txid
                width: parent.width
                horizontalAlignment: Qt.AlignHCenter
                text: qsTr("Your payment can be found by its identifyer: %1").arg(payment.txid)
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            RowLayout {
                id: txIdFeedback
                anchors.top: txidFeedbackLabel.bottom
                anchors.topMargin: 10
                anchors.horizontalCenter: parent.horizontalCenter
                ToolButton {
                    icon.source: "qrc:/edit-copy.svg"
                    onClicked: Pay.copyToClipboard(payment.txid);
                    text: qsTr("Copy")
                }
                ToolButton {
                    onClicked: Pay.openInExplorer(payment.txid);
                    text: qsTr("Internet")
                }
            }

            Label {
                anchors.verticalCenter: transactionComment.verticalCenter
                anchors.right: transactionComment.left
                anchors.rightMargin: 10
                text: qsTr("Comment") + ":"
            }
            Flowee.TextField {
                id: transactionComment
                anchors.top: txIdFeedback.bottom
                anchors.topMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                width: 400
                onTextChanged: payment.userComment = text
            }

            Flowee.Button {
                anchors.top: transactionComment.bottom
                anchors.topMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 20
                text: qsTr("Close")
                onClicked: {
                    payment.reset()
                    transactionComment.text = ""
                }
            }

            Behavior on opacity { NumberAnimation { } }
            Behavior on y { NumberAnimation { } }
            Behavior on color { ColorAnimation { } }
        }
    }

    // ============= Payment components  ===============

    /*
     * Destination.
     * The payment-output (address based) component.
     */
    Component {
        id: destinationFields
        Flowee.GroupBox {
            id: destinationPane
            property QtObject paymentDetail: null

            collapsable: paymentDetail.collapsable
            onEffectiveCollapsedChanged: paymentDetail.collapsed = effectiveCollapsed
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
                        amount = Pay.amountToStringPretty(paymentDetail.paymentAmount)
                                + " " + Pay.unitName;
                }
                else {
                    amount = Fiat.formattedPrice(paymentDetail.fiatAmount)
                }

                return qsTr("%1 to %2", "summary text to pay X-euro to address M")
                            .arg(amount).arg(ad);
            }

            Item {
                implicitWidth: parent.width
                implicitHeight: Math.max(contentColumn.height, warningArea.visible ? warningArea.height : 0)
                ColumnLayout {
                    id: contentColumn
                    width: parent.width
                    RowLayout {
                        width: parent.width
                        Flowee.TextField {
                            id: destination
                            focus: true
                            property bool addressOk: (addressType === Wallet.CashPKH || addressType === Wallet.CashSH)
                                                     || (paymentDetail.forceLegacyOk && (addressType === Wallet.LegacySH || addressType === Wallet.LegacyPKH))
                            property var addressType: Pay.identifyString(text);
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            onActiveFocusChanged: updateColor();
                            onAddressOkChanged: {
                                updateColor();
                                addressInfo.createInfo();
                            }
                            placeholderText: qsTr("Enter Bitcoin Cash Address")
                            text: destinationPane.paymentDetail.address
                            onTextChanged: {
                                destinationPane.paymentDetail.address = text
                                updateColor();
                                addressInfo.createInfo();
                            }

                            function updateColor() {
                                if (!activeFocus && text !== "" && !addressOk)
                                    color = Pay.useDarkSkin ? "#ff6568" : "red"
                                else
                                    color = mainWindow.palette.text
                            }
                        }
                        Label {
                            color: "green"
                            font.pixelSize: 24
                            text: destination.addressOk ? "✔" : " "
                        }
                    }
                    Flowee.LabelWithClipboard {
                        visible: text !== ""
                        Layout.fillWidth: true
                        text: paymentDetail.formattedTarget
                        horizontalAlignment: Qt.AlignRight
                        font.italic: true
                    }
                    Item {
                        id: addressInfo
                        width: parent.width
                        property QtObject info: null
                        visible: info != null

                        function createInfo() {
                            if (destination.addressOk) {
                                var address = paymentDetail.formattedTarget
                                if (address === "") // it didn't need reformatting
                                    address = paymentDetail.address
                                info = Pay.researchAddress(address, addressInfo)
                            }
                            else {
                                delete info;
                                info = null;
                            }
                        }

                        Label {
                            anchors.right: parent.right
                            font.italic: true
                            text: {
                                var info = addressInfo.info
                                if (info == null)
                                    return "";
                                if (portfolio.current.id === info.accountId)
                                    return qsTr("self", "payment to self")
                                return info.accountName
                            }
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

                Item {
                    id: warningArea
                    // BTC address entered warning.
                    visible: (destination.addressType === Wallet.LegacySH || destination.addressType === Wallet.LegacyPKH)
                             && paymentDetail.forceLegacyOk === false;

                    width: parent.width - 40
                    height: warningColumn.height + 20 + destination.height
                    Rectangle {
                        anchors.fill: warningColumn
                        anchors.margins: -7
                        color: warning.palette.window
                        border.width: 2
                        border.color: "red"
                        radius: 10
                    }
                    Flowee.ArrowPoint {
                        x: 20
                        anchors.bottom: warningColumn.top
                        anchors.bottomMargin: 4
                        rotation: -90
                        color: "red"
                    }

                    Column {
                        id: warningColumn
                        x: 10
                        y: destination.height + 10
                        width: parent.width
                        spacing: 10
                        Label {
                            font.bold: true
                            font.pixelSize: warning.font.pixelSize * 1.2
                            text: qsTr("Warning")
                        }
                        Label {
                            id: warning
                            width: parent.width
                            text: qsTr("This is a BTC address, which is an incompatible coin. Your funds could get lost and Flowee will have no way to recover them. Are you sure this is the right address?")
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }

                        RowLayout {
                            width: parent.width
                            Item {
                                width: 1; height: 1
                                Layout.fillWidth: true
                            }

                            Button {
                                text: qsTr("Continue")
                                onClicked: paymentDetail.forceLegacyOk = true
                            }
                            Button {
                                text: qsTr("Cancel")
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
            onEffectiveCollapsedChanged: paymentDetail.collapsed = effectiveCollapsed
            property QtObject paymentDetail: null
            title: qsTr("Coin Selector")
            summary: qsTr("Selected %1 %2 in %3 coins", "selected 2 BCH in 5 coins", paymentDetail.selectedCount)
                            .arg(Pay.amountToStringPretty(paymentDetail.selectedValue))
                            .arg(Pay.unitName)
                            .arg(paymentDetail.selectedCount)

            // make this tabs arrow-navigation go to our coinsListView.
            Component.onCompleted: listViewKeyHandler.target = coinsListView


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
                    var suggested = contentArea.height * 0.7
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
                            visible: locked && rowMouseArea.containsMouse
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
                        // only HD wallets can use cash-fusion
                        anchors.rightMargin: portfolio.current.isHDWallet ? 30 : 0
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

                        onClicked: (mouse)=> {
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
                    Flowee.CashFusionIcon {
                        id: fusedIcon
                        anchors.right: parent.right
                        anchors.verticalCenter: mainText.verticalCenter
                    }
                }
            }
        }
    }
}
