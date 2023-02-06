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


    Item { // data
        QRScanner {
            id: scanner
            scanType: QRScanner.PaymentDetails
            // autostart: true
            onFinished: {
                var rc = scanResult
                if (rc === "") { // scanning failed
                    thePile.pop();
                }
                else {
                    payment.targetAddress = rc
                    if (payment.isValid)
                        payment.prepare();
                    priceBch.forceActiveFocus();
                }
            }
        }
        Payment {
            id: payment
            account: portfolio.current
            fiatPrice: Fiat.price
            onIsValidChanged: if (isValid) prepare()

            // easier testing values (for when you don't have a camera)
            paymentAmount: 100000000
            targetAddress: "qrejlchcwl232t304v8ve8lky65y3s945u7j2msl45"
            userComment: "bla bla bla"
        }

        AccountSelector {
            id: accountSelector
            width: root.width
            x: -10 // to correct the indent added in the fullPage
            y: (root.height - height) / 2
            onSelectedAccountChanged: payment.account = selectedAccount
            selectedAccount: payment.account
        }
    }

    // if true, the bitcoin value is provided by the user (or QR), and the fiat follows
    // if false, the user edits the fiat price and the bitcoin value is calculated.
    // Notice that 'send all' overrules both and gets the data from the wallet-total
    property bool fiatFollowsSats: true

    Flowee.BitcoinValueField {
        id: priceBch
        y: root.fiatFollowsSats ? 5 : 68
        value: payment.paymentAmount
        focus: true
        fontPixelSize: size
        property double size: fiatFollowsSats ? 38 : commentLabel.font.pixelSize* 0.8
        onActiveFocusChanged: if (activeFocus) fiatFollowsSats = true
        Behavior on size { NumberAnimation { } }
        Behavior on y { NumberAnimation { } }
        Flowee.ObjectShaker { id: bchShaker } // 'shake' to give feedback on mistakes

        // this unchecks 'max' on user editing of the value
        onValueEdited: payment.paymentAmount = value
    }
    MouseArea {
        /* Since the valueField is centred but only allows clicking on its active surface,
          we provide this one to make it possible to click on the full width and activate it.
        */
        width: root.width
        height: priceBch.height
        y: priceBch.y
        onClicked: priceBch.forceActiveFocus();
    }

    Flowee.FiatValueField  {
        id: priceFiat
        value: Fiat.priceFor(priceBch.value, payment.fiatPrice);
        y: root.fiatFollowsSats ? 68 : 5
        focus: true
        fontPixelSize: size
        property double size: !fiatFollowsSats ? 38 : commentLabel.font.pixelSize * 0.8
        onActiveFocusChanged: if (activeFocus) fiatFollowsSats = false
        Behavior on size { NumberAnimation { } }
        Behavior on y { NumberAnimation { } }
        Flowee.ObjectShaker { id: fiatShaker }
        onValueEdited: payment.details[0].fiatAmount = value
    }

    Flowee.Label {
        id: commentLabel
        text: qsTr("Payment description" + ":")
        visible: userComment.text !== ""
        y: 100
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

    Rectangle {
        id: inputChooser
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: userComment.bottom
        border.width: 1
        border.color: root.palette.highlight
        color: root.palette.base
        width: inputs.width + 20
        height: 40
        radius: 15

        Row {
            id: inputs
            x: 10
            y: 7.5
            spacing: 16
            Image {
                id: logo
                source: "qrc:/bch.svg"
                width: 25
                height: 25
                smooth: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: priceBch.forceActiveFocus();
                }
            }

            Flowee.Label {
                text: Fiat.currencySymbolPost + Fiat.currencySymbolPrefix
                width: 24
                font.pixelSize: 32
                horizontalAlignment: Text.horizontalCenter
                anchors.baseline: logo.bottom

                MouseArea {
                    anchors.fill: parent
                    onClicked: priceFiat.forceActiveFocus();
                }
            }

            /* TODO
            Flowee.HamburgerMenu {
                wide: true
                y: 6
                MouseArea {
                    anchors.fill: parent
                    // onClicked:
                    // TODO show small menu with last 5 chosen fiat currencies
                    // and a 'more' item which leads to a separate page for all
                    // possible currencies.
                }
            }*/
        }

        Rectangle {
            color: root.palette.text
            opacity: 0.3
            radius: 6
            width: 35
            height: parent.height - 10
            y: 5
            x: root.fiatFollowsSats ? 5 : 45

            Behavior on x { NumberAnimation { } }
        }
    }



    Flowee.Label {
        id: errorLabel
        text: payment.error
        visible: payment.isValid
        color: Pay.useDarkSkin ? "#cc5454" : "#6a0c0c"
        anchors.bottom: walletNameBackground.top
        wrapMode: Text.Wrap
        anchors.bottomMargin: 10
        width: parent.width
    }

    Rectangle {
        id: walletNameBackground
        anchors.top: currentWalletLabel.top
        anchors.topMargin: -5
        width: parent.width
        anchors.bottom: currentWalletValue.bottom
        anchors.bottomMargin: -5
        color: root.palette.alternateBase

        MouseArea {
            anchors.fill: parent
            onClicked: (mouse) => {
                if (mouse.x < parent.width / 2) {
                    accountSelector.open();
                } else {
                    // TODO open popup on price.
                    // this should include a 'max-price' option like this;
                    // payment.details[0].maxSelected = checked
                }
            }
        }
    }

    Flowee.Label {
        id: currentWalletLabel
        text: payment.account.name
        visible: payment.account.isUserOwned
        anchors.baseline: parent.width > currentWalletLabel.width + currentWalletValue.width
                    ? currentWalletValue.baseline : undefined
        anchors.bottom: parent.width > currentWalletLabel.width + currentWalletValue.width
                    ? undefined : currentWalletValue.top
    }
    Flowee.BitcoinAmountLabel {
        id: currentWalletValue
        anchors.right: parent.right
        anchors.bottom: numericKeyboard.top
        anchors.bottomMargin: 10
        value: {
            var wallet = payment.account;
            return wallet.balanceConfirmed + wallet.balanceUnconfirmed;
        }
    }

    Flow {
        id: numericKeyboard
        anchors.bottom: slideToApprove.top
        anchors.bottomMargin: 15
        width: parent.width
        enabled: !payment.details[0].maxSelected
        Repeater {
            model: 12
            delegate: Item {
                width: numericKeyboard.width / 3
                height: textLabel.height + 20
                QQC2.Label {
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
                    // make dim when not enabled.
                    color: {
                        if (!enabled)
                            return Pay.useDarkSkin ? Qt.darker(palette.buttonText, 1.8) : Qt.lighter(palette.buttonText, 2);
                        return palette.windowText;
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
