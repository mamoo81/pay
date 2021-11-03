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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import QtQuick.Shapes 1.11 // for shape-path
import Flowee.org.pay 1.0
import "widgets" as Flowee

Pane {
    id: receivePane

    property QtObject account: portfolio.current
    property QtObject request: null
    onAccountChanged: {
        if (account == null)
            return;
        if (request == null)
            request = account.createPaymentRequest(receivePane)
        else
            request.switchAccount(portfolio.current);
    }

    function reset() {
        if (request.saveState !== PaymentRequest.Stored)
            request.destroy();
        request = account.createPaymentRequest(receivePane)
        description.text = "";
        bitcoinValueField.reset();
    }

    Label {
        id: instructions
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20
        text: qsTr("Share your QR code or copy address to receive")
        opacity: 0.5
    }

    Image {
        id: qrCode
        width: height
        height: {
            var h = parent.height - 220;
            return Math.min(h, 256)
        }
        source: "image://qr/" + receivePane.request.qr
        smooth: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: instructions.bottom
        anchors.topMargin: 20
        opacity: receivePane.request.state === PaymentRequest.Unpaid ? 1: 0

        MouseArea {
            anchors.fill: parent
            onClicked: {
                Pay.copyToClipboard(receivePane.request.qr)
                clipboardFeedback.opacity = 1
            }
        }

        Rectangle {
            id: clipboardFeedback
            opacity: 0
            width: feedbackText.width + 20
            height: feedbackText.height + 20
            radius: 10
            color: Pay.useDarkSkin ? "#333" : "#ddd"
            anchors.top: parent.bottom
            anchors.topMargin: -13
            anchors.horizontalCenter: parent.horizontalCenter

            Label {
                id: feedbackText
                x: 10
                y: 10
                text: qsTr("Copied to clipboard")
                wrapMode: Text.WordWrap
            }

            Behavior on opacity { OpacityAnimator {} }

            /// after 10 seconds, remove feedback.
            Timer {
                interval: 10000
                running: clipboardFeedback.opacity === 1
                onTriggered: clipboardFeedback.opacity = 0
            }
        }

        Behavior on opacity { OpacityAnimator {} }
    }

    // the "payment received" screen.
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: qrCode.bottom
        radius: 10
        gradient: Gradient {
            GradientStop {
                position: 0.6
                color: {
                    var state = receivePane.request.state;
                    if (state === PaymentRequest.PaymentSeen || state === PaymentRequest.Unpaid)
                        return receivePane.palette.base
                    if (state === PaymentRequest.DoubleSpentSeen)
                        return "#640e0f" // red
                    return "#3e8b4e" // in all other cases: green
                }
                Behavior on color { ColorAnimation {} }
            }
            GradientStop {
                position: 0.1
                color: receivePane.palette.base
            }
        }
        opacity: receivePane.request.state === PaymentRequest.Unpaid ? 0: 1

        // animating timer to indicate our checking the security of the transaction.
        // (i.e. waiting for the double spent proof)
        Item {
            id: feedback
            width: 160
            height: 160
            y: (parent.height - height) / 3 * 2
            visible: receivePane.request.state !== PaymentRequest.DoubleSpentSeen
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
                        sweepAngle: receivePane.request.state === PaymentRequest.Unpaid ? 0: 340

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
                var s = receivePane.request.state;
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
            visible: receivePane.request.state === PaymentRequest.DoubleSpentSeen
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
        anchors.top: qrCode.bottom
        anchors.topMargin: 30
        columns: 2
        Label {
            text: qsTr("Description") + ":"
        }
        TextField {
            id: description
            Layout.fillWidth: true
            enabled: receivePane.request.state === PaymentRequest.Unpaid
            onTextChanged: receivePane.request.message = text
        }

        Label {
            id: payAmount
            text: qsTr("Amount") + ":"
        }
        RowLayout {
            spacing: 10
            Flowee.BitcoinValueField {
                id: bitcoinValueField
                fontPtSize: payAmount.font.pointSize
                enabled: receivePane.request.state === PaymentRequest.Unpaid
                onValueChanged: receivePane.request.amount = value
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
                text: qsTr("Remember", "payment request")
                visible: receivePane.request.state === PaymentRequest.Unpaid || receivePane.request.state === PaymentRequest.DoubleSpentSeen
                onClicked: {
                    receivePane.request.stored = true
                    reset();
                }
            }
            Button {
                text: receivePane.request.state === PaymentRequest.Unpaid ? qsTr("Clear") : qsTr("Done")
                onClicked: {
                    reset();
                }
            }
        }
    }

    Flow {
        width: parent.width
        anchors.bottom: parent.bottom
        spacing: 10
        Repeater {
            model: portfolio.current.paymentRequests
            delegate: Rectangle {
                width: 70
                height: width
                radius: 25
                clip: true
                border.width: 6
                border.color: {
                    var state = modelData.state;
                    if (state === PaymentRequest.Unpaid)
                        return "#888888"
                    if (state === PaymentRequest.PaymentSeen)
                        return "yellow"
                    if (state === PaymentRequest.DoubleSpentSeen)
                        return "red"
                    // in all other cases:
                    return "green"
                }

                // don't show the one we are editing
                visible: modelData.saveState === PaymentRequest.Stored

                Text {
                    anchors.centerIn: parent
                    text: modelData.message
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.LeftButton
                    onClicked: paymentContextMenu.popup()
                    Menu {
                        id: paymentContextMenu
                        MenuItem {
                            text: qsTr("Delete")
                            onTriggered: modelData.stored = false;
                        }
                    }
                }
            }
        }
    }
}
