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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Flowee.org.pay
import "../Flowee" as Flowee

Pane {
    id: receivePane

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

    Label {
        id: instructions
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20
        text: qsTr("Share your QR code or copy address to receive")
        opacity: 0.5
    }

    Flowee.QRWidget {
        id: qr
        width: Math.min(parent.width, 360)
        anchors.top: instructions.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20
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

    // the "payment received" screen.
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: qr.bottom
        radius: 10
        gradient: Gradient {
            GradientStop {
                position: 0.6
                color: {
                    var state = request.state;
                    if (state === PaymentRequest.PaymentSeen || state === PaymentRequest.Unpaid)
                        return palette.base
                    if (state === PaymentRequest.DoubleSpentSeen)
                        return "#640e0f" // red
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
            y: (parent.height - height) / 3 * 2
            visible: request.state !== PaymentRequest.DoubleSpentSeen
            Shape {
                id: circleShape
                anchors.fill: parent
                opacity: progressCircle.sweepAngle === 340 ? 0 : 1
                x: 40
                ShapePath {
                    strokeWidth: 20
                    strokeColor: "#9ea0b0"
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 100; startY: 10

                    PathAngleArc {
                        id: progressCircle
                        centerX: 80
                        centerY: 80
                        radiusX: 70; radiusY: 70
                        startAngle: -80
                        sweepAngle: request.state === PaymentRequest.Unpaid ? 0: 340

                        Behavior on sweepAngle {  NumberAnimation { duration: Pay.dspTimeout } }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("Checking") // checking security
                }
                Behavior on opacity { OpacityAnimator {} }
            }

            Label {
                color: "green"
                text: "✔"
                opacity: 1 - circleShape.opacity
                font.pixelSize: 130
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
            }
        }

        Label {
            id: feedbackLabel
            text: {
                var s = request.state;
                if (s === PaymentRequest.DoubleSpentSeen)
                    // double-spent-proof received
                    return qsTr("Transaction high risk")
                if (s === PaymentRequest.PaymentSeen)
                    return qsTr("Payment Seen")
                if (s === PaymentRequest.PaymentSeenOk)
                    return qsTr("Payment Accepted")
                if (s === PaymentRequest.Confirmed)
                    return qsTr("Payment Settled")
                return "INTERNAL ERROR";
            }
            width: parent.width - 40
            anchors.verticalCenter: feedback.verticalCenter
            anchors.left: feedback.visible ? feedback.right : parent.left
            anchors.leftMargin: 20
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.pointSize: 20
        }
        Label {
            visible: request.state === PaymentRequest.DoubleSpentSeen
            anchors.top: feedbackLabel.bottom
            anchors.right: parent.right
            anchors.rightMargin: 10
            width: parent.width - 20
            horizontalAlignment: Qt.AlignRight
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            text: qsTr("Instant payment failed. Wait for confirmation. (double spent proof received)")
        }

        Behavior on opacity { OpacityAnimator {} }
    }

    // entry-fields
    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: qr.bottom
        anchors.topMargin: 30
        columns: 2
        Label {
            text: qsTr("Description") + ":"
        }
        Flowee.TextField {
            id: description
            Layout.fillWidth: true
            enabled: request.state === PaymentRequest.Unpaid
            onTextChanged: request.message = text
            focus: true
        }

        Label {
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
            Label {
                Layout.alignment: Qt.AlignBaseline
                anchors.baselineOffset: bitcoinValueField.baselineOffset
                text: Fiat.formattedPrice(bitcoinValueField.value, Fiat.price)
            }
        }

        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Pane {
                Layout.fillWidth: true
            }
            Button {
                text: request.state === PaymentRequest.Unpaid ? qsTr("Clear") : qsTr("Done")
                onClicked: {
                    request.clear();
                    request.account = portfolio.current;
                    description.text = "";
                    bitcoinValueField.value = 0;
                    request.start();
                }
            }
        }
    }
}
