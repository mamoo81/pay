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
import QtQuick.Shapes
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay

FocusScope {
    id: receiveTab
    property string icon: "qrc:/receive.svg"
    property string  title: qsTr("Receive")

    focus: true
    property QtObject account: portfolio.current
    property bool qrViewActive: true;
    property bool editViewActive: false

    PaymentRequest {
        id: request
        amount: priceInput.paymentBackend.paymentAmount
    }

    onAccountChanged: {
        var state = request.state;
        if (request.state === PaymentRequest.Unpaid) {
            // I can only change the wallet without cost
            // if no payment has been seen.
            request.account = account;
        }
    }

    onActiveFocusChanged: {
        // starting reserves an adderess, don't do that until
        // this tab is actually viewed.
        if (activeFocus)
            request.start();
    }

    Column {
        id: verticalTabs
        y: 50
        x: -10
        Repeater {
            model: ["qr-code", "edit-pen"]

            delegate: MouseArea {
                property bool active: index === 0
                onActiveChanged: {
                    let a = active;
                    if (a) // disable the other one
                        verticalTabs.children[(index + 1) % 2].active = false
                    // update page-scope state variables
                    if (index === 1)
                        a = !a;
                    qrViewActive = a;
                    editViewActive = !a;
                }
                width: 50
                height: 50
                onClicked: active = true;

                Rectangle {
                    anchors.fill: parent
                    color: palette.window
                }
                Rectangle {
                    x: 46
                    y: 5
                    width: 4
                    height: 40
                    color: palette.highlight
                    visible: active
                }
                Rectangle {
                    anchors.fill: parent
                    color: palette.highlight
                    visible: active
                    opacity: 0.15
                }
                Image {
                    anchors.fill: parent
                    anchors.margins: 10
                    source: "qrc:/" + modelData + (Pay.useDarkSkin ? "-light" : "") + ".svg"
                    smooth: true
                }
            }
        }
    }

    Flowee.Label {
        id: instructions
        y: 35 - height
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Share this QR to receive")
        opacity: editViewActive ? 0 : 0.5
        Behavior on opacity { OpacityAnimator { } }
    }

    Flowee.QRWidget {
        id: qr
        width: parent.width
        y: qrViewActive ? 40 : editBox.height
        x: qrViewActive || editBox.editingTextField ? 0 : 0 - width
        qrSize: qrViewActive ? 256 : 160
        textVisible: false
        qrText: request.qr

        Flowee.Label {
            visible: request.failReason !== PaymentRequest.NoFailure
            text: {
                var f = request.failReason;
                if (f === PaymentRequest.AccountEncrypted)
                    return qsTr("Encrypted Wallet");
                if (f === PaymentRequest.AccountImporting)
                    return qsTr("Import Running...");
                if (f === PaymentRequest.NoAccountSet)
                    return "No Account Set"; // not translated b/c cause is bug in QML
                return "";
            }
            anchors.centerIn: parent
            width: parent.width - 40
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.pointSize: 18
        }
        Behavior on y { NumberAnimation { } }
        Behavior on x { NumberAnimation { } }
    }

    // feedback-fields
    Column {
        width: parent.width
        spacing: 10
        opacity: qrViewActive ? 1 : 0
        y: instructions.height + qr.height + 30

        PageTitledBox {
            width: parent.width
            visible: request.message !== ""
            title: qsTr("Description")
            Flowee.Label {
                text: request.message
            }
        }
        PageTitledBox {
            width: parent.width
            visible: request.amount > 0
            title: qsTr("Amount", "requested amount of coin")
            Flowee.BitcoinAmountLabel {
                colorize: false
                value: request.amount
            }
        }
        PageTitledBox {
            width: parent.width
            title: qsTr("Address", "Bitcoin Cash address")
            Flowee.LabelWithClipboard {
                width: parent.width
                text: request.addressShort
                font.pixelSize: instructions.font.pixelSize * 0.9
            }
        }
        Flowee.Button {
            anchors.right: parent.right
            text: qsTr("Clear")
            enabled: description.text != "" || priceInput.paymentBackend.paymentAmount > 0;
            onClicked: {
                request.clear();
                request.account = portfolio.current;
                description.text = "";
                priceInput.paymentBackend.paymentAmount = 0;
                request.start();
            }
        }
        Behavior on opacity { OpacityAnimator { } }
    }

    // edit fields
    PageTitledBox {
        id: editBox
        width: parent.width - 50
        x: 50
        title: qsTr("Description")
        opacity: editViewActive ? 1 : 0
        enabled: editViewActive

        // little state-machine to switch between the two
        // input options
        property bool editingTextField: true

        onEnabledChanged: {
            // due to lack of a good statemachine, a bit hackish to
            // switch states properly.
            if (enabled) {
                if (editingTextField)
                    description.forceActiveFocus()
                else
                    priceInput.forceActiveFocus()
            }
            else {
                priceInput.focus = false;
                description.focus = false;
            }
        }

        Flowee.TextField {
            id: description
            width: parent.width
            onTextChanged: if (!inputMethodComposing) request.message = text
            onActiveFocusChanged: if (activeFocus) editBox.editingTextField = true
        }

        Behavior on opacity { OpacityAnimator { } }

        PriceInputWidget {
            id: priceInput
            fiatFollowsSats: false
            width: parent.width
            onActiveFocusChanged: if (activeFocus) editBox.editingTextField = false

            Connections {
                target: Fiat
                /*
                 * The price may change simply because the market moved and we re-cheked the server,
                 * but more important is the case where the user changed which currency they use.
                 */
                function onPriceChanged() {
                    // set the value again to trigger an updated exchange-rate calculation to happen.
                    var price = priceInput.editor.value;
                    if (priceInput.fiatFollowsSats)
                        priceInput.paymentBackend.paymentAmount = price;
                    else
                        priceInput.paymentBackend.paymentAmountFiat = price;
                }
            }
        }
    }

    NumericKeyboardWidget {
        id: numericKeyboard
        width: parent.width
        x: qrViewActive || editBox.editingTextField ? width + 10 : 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
    }

    // the "payment received" screen.
    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -10 // be actually edge to edge
        anchors.rightMargin: -10
        radius: 10
        gradient: Gradient {
            GradientStop {
                position: 0.6
                color: {
                    var state = request.state;
                    if (state === PaymentRequest.PaymentSeen || state === PaymentRequest.Unpaid)
                        return palette.base
                    if (state === PaymentRequest.DoubleSpentSeen)
                        return mainWindow.errorRedBg
                    return "#3e8b4e" // in all other cases: green
                }
                Behavior on color { ColorAnimation {} }
            }
            GradientStop {
                position: 0.1
                color: palette.base
            }
        }
        opacity: request.state === PaymentRequest.Unpaid ? 0: 1
        enabled: opacity > 0
        visible: opacity > 0

        // animating timer to indicate our checking the security of the transaction.
        // (i.e. waiting for the double spent proof)
        Item {
            id: feedback
            width: 160
            height: 160

            y: 60
            x: 30
            visible: request.state !== PaymentRequest.DoubleSpentSeen
            Shape {
                id: circleShape
                anchors.fill: parent
                opacity: request.state === PaymentRequest.Unpaid ? 0: 1
                x: 40
                ShapePath {
                    strokeWidth: 20
                    strokeColor: request.state === PaymentRequest.PaymentSeenOk
                        ? "green" : "#9ea0b0"
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 100; startY: 10

                    PathAngleArc {
                        id: progressCircle
                        centerX: 80
                        centerY: 80
                        radiusX: 70; radiusY: 70
                        startAngle: -20
                        sweepAngle: request.state === PaymentRequest.Unpaid ? 0: 340

                        Behavior on sweepAngle {  NumberAnimation { duration: Pay.dspTimeout } }
                    }
                }

                Behavior on opacity { OpacityAnimator {} }
            }
            Flowee.Label {
                color: "green"
                text: "✔"
                opacity: {
                    if (request.state === PaymentRequest.PaymentSeenOk
                            || request.state === PaymentRequest.Confirmed)
                        return 1;
                    return 0;
                }
                font.pixelSize: 130
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
            }
        }
        // textual feedback
        Item {
            clip: true
            anchors.top: feedback.top
            anchors.left: feedback.right
            anchors.right: parent.right
            height: 160
            opacity: {
                var s = request.state
                if (s == PaymentRequest.PartiallyPaid
                        || s === PaymentRequest.PaymentSeen)
                    return 1;
                return 0;
            }
            Flowee.Label {
                id: label1
                text: qsTr("Payment Seen")
                x: request.state === PaymentRequest.Unpaid ? (0 - width - 10) : 0
                y: 30
                Behavior on x { NumberAnimation { } }
            }
            Flowee.Label {
                id: label2
                text: qsTr("Checking...") // checking security
                x: (label1.x > (0 - label1.width) + 20) ? 0 : 0 - width - 10
                y: label1.height + 8 + 30
                Behavior on x { NumberAnimation { } }
            }
            Behavior on opacity { OpacityAnimator { duration: 1000 } }
        }


        Flowee.Label {
            id: feedbackLabel
            text: {
                var s = request.state;
                if (s === PaymentRequest.DoubleSpentSeen) {
                    // double-spent-proof received
                    return qsTr("Transaction high risk");
                }
                if (s === PaymentRequest.PartiallyPaid)
                    return qsTr("Partially Paid");
                if (s === PaymentRequest.PaymentSeen)
                    return qsTr("Payment Seen");
                if (s === PaymentRequest.PaymentSeenOk)
                    return qsTr("Payment Accepted");
                if (s === PaymentRequest.Confirmed)
                    return qsTr("Payment Settled");
                return "INTERNAL ERROR";
            }
            width: parent.width - 40
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: feedback.bottom
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.pointSize: 20
        }
        Flowee.Label {
            visible: request.state === PaymentRequest.DoubleSpentSeen
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: feedbackLabel.bottom
            anchors.topMargin: 20
            width: parent.width - 40
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            text: qsTr("Instant payment failed. Wait for confirmation. (double spent proof received)")
        }

        Behavior on opacity { OpacityAnimator {} }

        Flowee.Button {
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 400
            text: qsTr("Continue")
            onClicked: {
                request.clear();
                description.text = "";
                priceInput.paymentBackend.paymentAmount = 0;
                request.account = portfolio.current;
                request.start();
                switchToTab(0); // go to the 'main' tab.
            }
        }
    }
}
