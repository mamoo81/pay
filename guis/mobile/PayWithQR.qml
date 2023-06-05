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
            priceInput.forceActiveFocus();
            priceInput.fiatFollowsSats = false
        }
    }
    menuItems: {
        // only have menu items as long as we are effectively
        // showing this page and not some overlay.
        if (payment.broadcastStatus === Payment.NotStarted && !scanner.isScanning) {
            // a QR _with_ a bch-amount will turn off editing of amount-to-send
            if (allowEditAmount)
                return [ sendAllAction ];
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
            autostart: true
            onFinished: {
                var rc = scanResult
                if (rc === "") { // scanning interrupted
                    thePile.pop();
                    return;
                }
                // Take the entire QR-url and let the Payment object parse it.
                // this updates things like amount, comment and indeed address.
                payment.targetAddress = rc
                if (payment.formattedTargetAddress == "") {
                    // that means that the address is invalid.
                    scannedUrlFaultyDialog.open();
                }

                // should the price be included in the QR code, don't show editing widgets.
                root.allowEditAmount = payment.paymentAmount <= 0;
            }
        }
        Payment {
            id: payment
            account: portfolio.current
            fiatPrice: Fiat.price
            autoPrepare: true
            instaPay: true

            // easier testing values (for when you don't have a camera)
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
        text: qsTr("Payment description") + ":"
        visible: userComment.text !== ""
        anchors.top: priceInput.bottom
        anchors.topMargin: 10
    }
    Flowee.Label {
        id: userComment
        text: payment.userComment
        visible: text !== ""
        color: palette.highlight
        font.italic: true
        anchors.top: commentLabel.bottom
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

        balanceActions: [ sendAllAction ]
    }

    NumericKeyboardWidget {
        id: numericKeyboard
        anchors.bottom: slideToApprove.top
        anchors.bottomMargin: 15
        width: parent.width
        enabled: !payment.details[0].maxSelected
        x: allowEditAmount ? 0 : 0 - width

        Behavior on x { NumberAnimation { } }
    }

    PageTitledBox {
        anchors.top: numericKeyboard.top
        width: parent.width
        visible: !allowEditAmount && showTargetAddress.checked
        title: qsTr("Destination Address")
        Flowee.LabelWithClipboard {
            text: payment.niceAddress
            width: parent.width
            font.pixelSize: mainWindow.font.pixelSize * 0.9
        }
    }

    SlideToApprove {
        id: slideToApprove
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: parent.width
        enabled: payment.isValid && payment.txPrepared
        onActivated: payment.broadcast()
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
