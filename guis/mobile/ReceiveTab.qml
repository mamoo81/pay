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

    /*
       TZ: I apologize for the design, I made it when I just had learned QML / cpp mixing.
       This is a quick port from the desktop one. In general this component needs
       a redesign so we avoid lots of null pointer checks and similar logic that really
       should be hidden from the UI layer.
     */

    onAccountChanged: {
        if (qr.request != null) {
            if (qr.request.state === PaymentRequest.Unpaid)
                qr.request.switchAccount(portfolio.current);
            else
                qr.request = null;
        }
    }
    onActiveFocusChanged: {
        if (activeFocus && qr.request == null)  {
            qr.request = account.createPaymentRequest(receiveTab)
        }
    }

    function reset() {
        if (qr.request.saveState !== PaymentRequest.Stored)
            qr.request.destroy();
        qr.request = account.createPaymentRequest(receiveTab)
        description.text = "";
        // bitcoinValueField.reset();
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
                    if (qr.request == null)
                        return "red"
                    var state = qr.request.state;
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
        opacity: qr.request == null ? 0 : (qr.request.state === PaymentRequest.Unpaid ? 0: 1)

        // animating timer to indicate our checking the security of the transaction.
        // (i.e. waiting for the double spent proof)
        Item {
            id: feedback
            width: 160
            height: 160
            y: (parent.height - height) / 3 * 2
            visible: qr.request == null || qr.request.state !== PaymentRequest.DoubleSpentSeen
            Shape {
                id: circleShape
                anchors.fill: parent
                opacity: progressCircle.sweepAngle <= 340 ? 0 : 1
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
                        sweepAngle: qr.request == null || qr.request.state === PaymentRequest.Unpaid ? 0: 340

                        Behavior on sweepAngle {  NumberAnimation { duration: Pay.dspTimeout } }
                    }
                }

                Flowee.Label {
                    anchors.centerIn: parent
                    text: qsTr("Checking") // checking security
                }
                Behavior on opacity { OpacityAnimator {} }
            }

            Flowee.Label {
                color: "green"
                text: "✔"
                opacity: 1 - circleShape.opacity
                font.pixelSize: 130
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
            }
        }

        Flowee.Label {
            id: feedbackLabel
            text: {
                if (qr.request == null)
                    return "";
                var s = qr.request.state;
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
        Flowee.Label {
            visible: qr.request == null || qr.request.state === PaymentRequest.DoubleSpentSeen
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
            enabled: qr.request != null && qr.request.state === PaymentRequest.Unpaid
            onTextChanged: qr.request.message = text
        }

        /*
        Flowee.Label {
            id: payAmount
            text: qsTr("Amount") + ":"
        }
        RowLayout {
            spacing: 10
            Flowee.BitcoinValueField {
                id: bitcoinValueField
                enabled: qr.request != null && qr.request.state === PaymentRequest.Unpaid
                onValueChanged: qr.request.amount = value
            }
            Flowee.Label {
                Layout.alignment: Qt.AlignBaseline
                anchors.baselineOffset: bitcoinValueField.baselineOffset
                text: Fiat.formattedPrice(bitcoinValueField.value, Fiat.price)
            }
        }
        */

        Flowee.Button {
            Layout.alignment: Qt.AlignRight
            text: qr.request == null || qr.request.state === PaymentRequest.Unpaid ? qsTr("Clear") : qsTr("Done")
            onClicked: reset();
        }
    }

}
