/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import Flowee.org.pay;

Page {
    id: root
    headerText: qsTr("Approve Payment")

    property QtObject sendAllAction: QQC2.Action {
        checkable: true
        checked: payment.details[0].maxSelected
        text: qsTr("Send All", "all money in wallet")
        onTriggered: {
            payment.details[0].maxSelected = checked
            if (payment.isValid)
                payment.prepare(); // auto-prepare doesn't act on changes done on the details.
        }
    }
    property QtObject showTargetAddress: QQC2.Action {
        checkable: true
        text: qsTr("Show Address", "to show a bitcoincash address")
    }
    property QtObject editPrice: QQC2.Action {
        text: qsTr("Edit Amount", "Edit amount of money to send")
        onTriggered: {
            root.closeHeaderMenu();
            // this action can only ever be used to start editing.
            root.allowEditAmount = true;
            priceInput.fiatFollowsSats = false
            priceInput.takeFocus()
        }
        checked: root.allowEditAmount
    }
    menuItems: {
        // only have menu items as long as we are effectively
        // showing this page and not some overlay.
        if (payment.broadcastStatus === Payment.NotStarted && !scanner.isScanning) {
            // a QR _with_ a bch-amount will turn off editing of amount-to-send
            if (allowEditAmount)
                return [ showTargetAddress, sendAllAction ];
            else
                return [ showTargetAddress, editPrice ];
        }
        return [];
    }
    // if true, show widgets to edit the amount-to-send
    property bool allowEditAmount: true

    Item { // data
        QRScanner {
            id: scanner
            scanType: QRScanner.PaymentDetails
            autostart: Pay.paymentProtocolRequest === ""
            onFinished: {
                var rc = scanResult
                if (rc === "") { // scanning interrupted
                    thePile.pop();
                    return;
                }
                // if the scanner got bypassed with a 'paste' then
                // we don't allow instapay
                if (resultSource === QRScanner.Clipboard)
                    payment.instaPay = false;

                // Take the entire QR-url and let the Payment object parse it.
                // this updates things like amount, comment and indeed address.
                let success = payment.pasteTargetAddress(rc);
                if (!success)
                    scannedUrlFaultyDialog.open();

                // should the price be included in the QR code, don't show editing widgets.
                root.allowEditAmount = payment.paymentAmount <= 0;
                if (root.allowEditAmount)
                    priceInput.takeFocus();
                else
                    root.takeFocus();
            }
        }
        Payment {
            id: payment
            account: portfolio.current
            fiatPrice: Fiat.price
            autoPrepare: true
            instaPay: true

            Component.onCompleted: {
                /*
                  The application can be started with a click on a payment link,
                  in that case the link gets made available in the following property
                  and we start a payment protocol with the value.
                  Afterwards we reset the property to avoid the next opening of this
                  screen repeating the payment.
                 */

                var paymentProtcolUrl = Pay.paymentProtocolRequest;
                if (paymentProtcolUrl !== "") {
                    scanner.autostart = false;
                    payment.targetAddress = paymentProtcolUrl;
                    Pay.paymentProtocolRequest = "";
                    root.allowEditAmount = payment.paymentAmount <= 0;
                }
            }

            // easier testing values (for when you don't have a camera)
            //    ps. in case of testing, you want above instaPay: property => false!!
            // paymentAmount: 100000000
            // targetAddress: "qrejlchcwl232t304v8ve8lky65y3s945u7j2msl45"
            // userComment: "bla bla bla"
        }
        Flowee.Dialog {
            id: scannedUrlFaultyDialog
            title: qsTr("Invalid QR code")
            standardButtons: QQC2.DialogButtonBox.Close
            onRejected: thePile.pop(); // remove this entire page
            contentComponent: dialogForFaultyUrl
        }
        Component {
            id: dialogForFaultyUrl
            Column {
                width: parent.width
                spacing: 10
                Flowee.Label {
                    id: mainText
                    width: parent.width
                    text: qsTr("I don't understand the scanned code. I'm sorry, I can't start a payment.")
                    wrapMode: Text.Wrap
                }
                Flowee.Label {
                    text: qsTr("details")
                    font.italic: true
                    color: palette.link
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: detailsLabel.visible = true;
                    }
                }
                Flowee.Label {
                    id: detailsLabel
                    text: qsTr("Scanned text: <pre>%1</pre>").arg(scanner.scanResult);
                    visible: false
                    font.pixelSize: mainText.pixelSize * 0.8
                    wrapMode: Text.Wrap
                    width: parent.width
                }
            }
        }
    }

    PriceInputWidget {
        id: priceInput // when the price / amount is editable
        paymentBackend: payment
        width: parent.width
        x: allowEditAmount ? 0 : 0 - width

        Behavior on x { NumberAnimation { } }
    }
    Item {
        id: priceFeedback // when the price / amount is given by the scanned QR
        width: parent.width
        x: allowEditAmount ?  0 - width : 0
        y: 10
        height: col.height

        Column {
            id: col
            width: parent.width
            Flowee.Label {
                id: fiatPrice
                anchors.horizontalCenter: parent.horizontalCenter
                text: Fiat.formattedPrice(payment.paymentAmountFiat)
                font.pixelSize: 38
            }

            Flowee.BitcoinAmountLabel {
                value: payment.paymentAmount
                colorize: false
                showFiat: false
                font.pixelSize: mainWindow.font.pixelSize * 0.8
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        Behavior on x { NumberAnimation { } }
    }

    Flowee.Label {
        id: commentLabel
        text: qsTr("Payment description")
        visible: userComment.visible
        y: {
            if (!userComment.visible)
                return 0;
            if (root.allowEditAmount) { // the numpad is on
                /*
                  position based on available space and how many items
                  need to be visible.
                  If invisible, place at -100
                 */
                let space = walletNameBackground.y - (priceInput.y + priceInput.height)
                if (destinationAddress.visible)
                    space -= destinationAddress.height + 10
                space -= userComment.height + 10;
                if (space < height + 10)
                    return -100;
                return priceInput.y + priceInput.height + 10;
            }
            return walletNameBackground.y + walletNameBackground.height + 10;
        }
    }
    Flowee.Label {
        id: userComment
        text: payment.userComment
        visible: text !== "" && !errorLabel.visible
        color: palette.highlight
        font.italic: true
        y: {
            if (!visible)
                return 0;
            if (commentLabel.y < 0)
                return priceInput.y + priceInput.height + 10;
            return commentLabel.y + commentLabel.height + 10;
        }
    }

    Flowee.Label {
        id: destinationAddressHeader
        text: qsTr("Destination Address")
        visible: destinationAddress.visible

        y: {
            if (!visible)
                return 0;
            if (root.allowEditAmount) { // the numpad is on
                let space = walletNameBackground.y - (priceInput.y + priceInput.height)
                if (userComment.visible)
                    space -= userComment.height + 10
                if (commentLabel.visible)
                    space -= commentLabel.height + 10
                space -= destinationAddress.height + 10;
                if (space < height + 10)
                    return -100;
                if (userComment.visible)
                    return userComment.y + userComment.height + 10;
                return priceInput.y + priceInput.height + 10;
            }
            if (userComment.visible)
                return userComment.y + userComment.height + 10;
            return walletNameBackground.y + walletNameBackground.height + 10;
        }
    }
    Flowee.LabelWithClipboard {
        id: destinationAddress
        text: payment.niceAddress
        width: parent.width
        visible: showTargetAddress.checked && !errorLabel.visible
        font.pixelSize: mainWindow.font.pixelSize * 0.9
        y: {
            if (!visible)
                return 0;
            if (destinationAddressHeader.y < 0){
                if (userComment.visible)
                    return userComment.y + userComment.height + 10;
                return priceInput.y + priceInput.height + 10;
            }
            return destinationAddressHeader.y + destinationAddressHeader.height + 10;
        }
    }

    Rectangle {
        visible: errorLabel.text !== ""
        color: mainWindow.errorRedBg
        radius: 10
        width: parent.width
        anchors.bottom: walletNameBackground.top
        anchors.bottomMargin: 6
        height: errorLabel.height + 20

        Flowee.Label {
            id: errorLabel
            text: payment.error
            wrapMode: Text.Wrap
            x: 10
            y: 10
            width: parent.width - 20
            horizontalAlignment: Qt.AlignHCenter
        }

        MouseArea {
            anchors.fill: parent
            // just here to catch mouse clicks.
        }
    }

    AccountSelectorWidget {
        id: walletNameBackground
        anchors.bottom: numericKeyboard.top
        anchors.bottomMargin: 10

        balanceActions: {
            if (editPrice.checked)
                return [ sendAllAction ];
            return [];
        }
    }

    NumericKeyboardWidget {
        id: numericKeyboard
        anchors.bottom: slideToApprove.top
        anchors.bottomMargin: 15
        width: parent.width
        enabled: !payment.details[0].maxSelected
        x: allowEditAmount ? 0 : 0 - width
        dataInput: priceInput

        Behavior on x { NumberAnimation { } }
    }

    SlideToApprove {
        id: slideToApprove
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: parent.width
        enabled: payment.isValid && payment.txPrepared
        onActivated: payment.markUserApproved()
        visible: payment.account.isDecrypted || !payment.account.needsPinToPay
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: warningsColumn.implicitHeight
        enabled: warningsColumn.implicitHeight > 0

        Column {
            id: warningsColumn
            width: parent.width
            Repeater {
                model: payment.warnings

                Rectangle {
                    y: 8
                    width: root.width - 16
                    height: Math.max(75, Math.max(warningIcon.height, warningText.height) + 20)
                    radius: 20
                    color: palette.alternateBase
                    border.width: 1
                    border.color: palette.midlight

                    Rectangle { // placeholder icon
                        id: warningIcon
                        x: 20
                        width: 40
                        height: 40
                        radius: 20
                        color: mainWindow.errorRedBg
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Flowee.Label {
                        id: warningText
                        text: modelData
                        wrapMode: Text.Wrap
                        anchors.left: warningIcon.right
                        anchors.leftMargin: 10
                        anchors.right: closeIcon.left
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Flowee.CloseIcon {
                        id: closeIcon
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 10
                        onClicked: payment.clearWarnings();
                    }
                }
            }
        }
    }

    Item {
        id: decryptButton
        visible: !slideToApprove.visible
        anchors.fill: slideToApprove

        Image {
            id: lockIcon
            source: "qrc:/lock" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
            x: 10
            y: 5
            width: 50
            height: 50
            smooth: true
        }

        Flowee.Button {
            height: parent.height - 10
            width: parent.width - 80
            x: 70
            y: 5
            text: qsTr("Unlock Wallet")
            onClicked: thePile.push(unlockInPage)
        }

        Component {
            id: unlockInPage
            Page {
                headerText: payment.account.name
                UnlockWalletPanel {
                    anchors.fill: parent
                    anchors.margins: 10

                    account: payment.account
                    Connections {
                        target: payment.account
                        function onIsDecryptedChanged() {
                            if (payment.account.isDecrypted)
                                thePile.pop()
                        }
                    }
                }
            }
        }
    }

    Flowee.BroadcastFeedback {
        id: broadcastPage
        anchors.leftMargin: -10 // go against the margins Page gave us to show more fullscreen.
        anchors.rightMargin: -10
        onStatusChanged: {
            if (status !== "")
                root.headerText = status;
        }

        onCloseButtonPressed: {
            var mainView = thePile.get(0);
            mainView.currentIndex = 0; // go to the 'main' tab.
            thePile.pop();
        }
    }
}
