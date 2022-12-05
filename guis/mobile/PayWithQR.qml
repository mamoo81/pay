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
            onFinished: payment.targetAddress = scanResult
        }
        Payment {
            id: payment
        }
    }

    Flowee.BitcoinAmountLabel {
        id: priceBch
        value: payment.paymentAmount
        fontPixelSize: 38
        anchors.horizontalCenter: parent.horizontalCenter
        y: 30
        colorize: false
        showFiat: false
    }
    Flowee.Label {
        id: priceFiat
        text: Fiat.formattedPrice(payment.paymentAmount, Fiat.price)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: priceBch.bottom
        anchors.topMargin: 12
        color: Qt.darker(palette.text, 1.2)
    }


    Flowee.Label {
        id: commentLabel
        text: qsTr("Payment description" + ":")
        visible: userComment.text !== ""
        color: palette.highlight

        anchors.top: priceFiat.bottom
        anchors.topMargin: 30
    }
    Flowee.Label {
        id: userComment
        text: payment.userComment
        visible: text !== ""
        font.italic: true
        anchors.top: commentLabel.bottom
        anchors.topMargin: 5
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
        width: parent.width
        Repeater {
            model: 12
            delegate: Item {
                width: numericKeyboard.width / 3
                height: text.height + 20
                Flowee.Label {
                    id: text
                    anchors.centerIn: parent
                    font.pixelSize: 28
                    text: {
                        if (index < 9)
                            return index + 1;
                        if (index === 9)
                            return Locale.decimalPoint
                        if (index === 10)
                            return "0"
                        if (index === 11)
                            return "<-" // TODO use a backspace icon instead.
                    }
                }
            }
        }
    }
    Item {
        id: slideToApprove
        width: parent.width
        height: 60
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        Rectangle {
            x: 30
            width: parent.height
            height: width
            radius: width / 2
            color: root.palette.window
        }
        Rectangle {
            x: parent.width - width - 30
            width: parent.height
            height: width
            radius: width / 2
            color: root.palette.window
        }
        Rectangle {
            x: 30 + parent.height / 2
            width: parent.width - 30 - 30 - parent.height
            height: parent.height
            color: root.palette.window
        }
        Flowee.Label {
            text: qsTr("SLIDE TO SEND")
            anchors.centerIn: parent
        }

        Rectangle {
            width: parent.height - 5
            height: width
            radius: width / 2
            // opacity: 0.8
            x: 35
            y: 2.5
            color: Pay.useDarkSkin ? mainWindow.floweeGreen : mainWindow.floweeBlue
        }

    }

}
