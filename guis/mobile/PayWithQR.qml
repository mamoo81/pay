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
    menuItems: {
        // only have menu items as long as we are effectively
        // showing this page and not some overlay.
        if (payment.broadcastStatus === Payment.NotStarted && !scanner.isScanning)
            return [ sendAllAction ];
        return [];
    }

    Item { // data
        QRScanner {
            id: scanner
            scanType: QRScanner.PaymentDetails
            autostart: true
            onFinished: {
                var rc = scanResult
                if (rc === "") { // scanning failed
                    thePile.pop();
                }
                else {
                    payment.targetAddress = rc
                    priceInput.forceActiveFocus();
                    priceInput.fiatFollowsSats = false
                }
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
    }

    PriceInputWidget {
        id: priceInput
        paymentBackend: payment
        width: parent.width
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
