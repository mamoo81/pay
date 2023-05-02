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
import QtQuick.Shapes
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import Flowee.org.pay

FocusScope {
    id: receiveTab
    property string icon: "qrc:/receive.svg"
    property string  title: qsTr("Receive")

    focus: true
    property QtObject account: portfolio.current

    PaymentRequest {
        id: request
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
        if (activeFocus)
            request.start();
    }

    Flowee.Label {
        id: instructions
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20
        text: qsTr("Share this QR to receive")
        opacity: 0.5
    }

    Flowee.QRWidget {
        id: qr
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: instructions.bottom
        anchors.topMargin: 20
        width: parent.width
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
    }

    // entry-fields
    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: qr.bottom
        anchors.topMargin: 30
        Flowee.Label {
            text: qsTr("Description") + ":"
        }
        Flowee.TextField {
            id: description
            Layout.fillWidth: true
            enabled: request.state === PaymentRequest.Unpaid
            onTextChanged: request.message = text
        }

        Flowee.Label {
            id: payAmount
            text: qsTr("Amount") + ":"
        }
        RowLayout {
            spacing: 10
            Flowee.BitcoinValueField {
                id: bitcoinValueField
                enabled: request.state === PaymentRequest.Unpaid
                onValueChanged: request.amount = value
            }
            Flowee.Label {
                Layout.alignment: Qt.AlignBaseline
                anchors.baselineOffset: bitcoinValueField.baselineOffset
                text: Fiat.formattedPrice(bitcoinValueField.value, Fiat.price)
            }
        }
        Flowee.Button {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Clear")
            onClicked: {
                request.clear();
                request.account = portfolio.current;
                description.text = "";
                bitcoinValueField.value = 0;
                request.start();
            }
        }
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
                bitcoinValueField.value = 0;
                request.account = portfolio.current;
                request.start();
            }
        }
    }
}
