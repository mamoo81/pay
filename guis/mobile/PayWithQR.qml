/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee
import Flowee.org.pay;

Page {
    id: root
    headerText: qsTr("Approve Payment")

    Item { // data
        QRScanner {
            id: scanner
            scanType: QRScanner.PaymentDetails
            Component.onCompleted: scanner.start();
            onFinished: {
                var rc = scanResult
                if (rc === "") { // scanning failed
                    thePile.pop();
                }
                else {
                    payment.targetAddress = rc
                    if (payment.isValid)
                        payment.prepare();
                }
            }
        }
        Payment {
            id: payment
            account: portfolio.current
            fiatPrice: Fiat.price
            onIsValidChanged: if (isValid) prepare()

            // easier testing values (for when you don't have a camera)
            // paymentAmount: 1000000
            // targetAddress: "qrejlchcwl232t304v8ve8lky65y3s945u7j2msl45"
        }
    }

    // if true, the bitcoin value is provided by the user (or QR), and the fiat follows
    // if false, the user edits the fiat price and the bitcoin value is calculated.
    property bool fiatFollowsSats: true

    Flowee.BitcoinValueField {
        id: priceBch
        y: 30
        value: payment.paymentAmount
        focus: true
        fontPixelSize: size
        property double size: fiatFollowsSats ? 38 : commentLabel.font.pixelSize
        onActiveFocusChanged: if (activeFocus) fiatFollowsSats = true
        Behavior on size { NumberAnimation { } }
        Flowee.ObjectShaker { id: bchShaker }

        onValueEdited: payment.paymentAmount = value
    }
    Flowee.FiatValueField  {
        id: priceFiat
        value: Fiat.priceFor(payment.paymentAmount)
        anchors.top: priceBch.bottom
        anchors.topMargin: 18
        focus: true
        fontPixelSize: size
        property double size: !fiatFollowsSats ? 38 : commentLabel.font.pixelSize
        onActiveFocusChanged: if (activeFocus) fiatFollowsSats = false
        Behavior on size { NumberAnimation { } }
        Flowee.ObjectShaker { id: fiatShaker }
    }

    Flowee.Label {
        id: commentLabel
        text: qsTr("Payment description" + ":")
        visible: userComment.text !== ""

        anchors.top: priceFiat.bottom
        anchors.topMargin: 30
    }
    Flowee.Label {
        id: userComment
        text: payment.userComment
        visible: text !== ""
        color: palette.highlight
        font.italic: true
        anchors.top: commentLabel.bottom
        anchors.topMargin: 5
    }
    Flowee.Label {
        id: errorLabel
        text: payment.error
        visible: payment.isValid
        color: Pay.useDarkSkin ? "#cc5454" : "#6a0c0c"
        anchors.top: userComment.bottom
        wrapMode: Text.Wrap
        anchors.topMargin: 20
        width: parent.width
    }

    Flowee.Label {
        id: currentWalletLabel
        text: portfolio.current.name
        anchors.baseline: parent.width > currentWalletLabel.width + currentWalletValue.width
                    ? currentWalletValue.baseline : undefined
        anchors.bottom: parent.width > currentWalletLabel.width + currentWalletValue.width
                    ? undefined : currentWalletValue.top
    }
    Flowee.BitcoinAmountLabel {
        id: currentWalletValue
        anchors.right: parent.right
        anchors.bottom: numericKeyboard.top
        anchors.bottomMargin: 20
        value: {
            var wallet = portfolio.current;
            return wallet.balanceConfirmed + wallet.balanceUnconfirmed;
        }
    }

    Flow {
        id: numericKeyboard
        anchors.bottom: slideToApprove.top
        anchors.bottomMargin: 15
        width: parent.width
        Repeater {
            model: 12
            delegate: Item {
                width: numericKeyboard.width / 3
                height: textLabel.height + 20
                Flowee.Label {
                    id: textLabel
                    anchors.centerIn: parent
                    font.pixelSize: 28
                    text: {
                        if (index < 9)
                            return index + 1;
                        if (index === 9)
                            return Qt.locale().decimalPoint
                        if (index === 10)
                            return "0"
                        // if (index === 11)
                            return "<-" // TODO use a backspace icon instead.
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        let editor = fiatFollowsSats ? priceBch.money : priceFiat.money;
                        if (index < 9) // these are digits
                            var shake = !editor.insertNumber("" + (index + 1));
                        else if (index === 9)
                            shake = !editor.addSeparator()
                        else if (index === 10)
                            shake = !editor.insertNumber("0");
                        else
                            shake = !editor.backspacePressed();
                        if (shake)
                            fiatFollowsSats ? bchShaker.start() : fiatShaker.start();
                    }
                }
            }
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

        onCloseButtonPressed: thePile.pop();
    }
}
