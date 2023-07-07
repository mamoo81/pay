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
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Shapes
import "../ControlColors.js" as ControlColors
import Flowee.org.pay


// screen to show the result of a Payment broadcast.
// Please notice you need to have a 'payment' object
// in the parents namespace.
// Also, this screen won't do anything until you actually
// call broadcast on the payment.
QQC2.Control {
    id: root
    anchors.fill: parent

    signal closeButtonPressed;
    property string status: ""

    states: [
        State {
            name: "notStarted"
            when: payment.broadcastStatus === Payment.NotStarted
        },
        State {
            name: "preparing"
            when: payment.broadcastStatus === Payment.TxOffered
            PropertyChanges { target: background;
                opacity: 1
                y: 0
            }
            PropertyChanges { target: progressCircle; sweepAngle: 90 }
            StateChangeScript { script: ControlColors.applyLightSkin(root) }
        },
        State {
            name: "sent1" // sent to only one peer
            extend: "preparing"
            when: payment.broadcastStatus === Payment.TxSent1
            PropertyChanges { target: progressCircle; sweepAngle: 150 }
            PropertyChanges { target: root; status:  qsTr("Sending Payment") }
        },
        State {
            name: "waiting" // waiting for possible rejection.
            when: payment.broadcastStatus === Payment.TxWaiting
            extend: "preparing"
            PropertyChanges { target: progressCircle; sweepAngle: 320 }
        },
        State {
            name: "success" // no reject, great success
            when: payment.broadcastStatus === Payment.TxBroadcastSuccess
            extend: "preparing"
            PropertyChanges { target: progressCircle
                sweepAngle: 320
                startAngle: -20
            }
            PropertyChanges { target: checkShape; opacity: 1 }
            PropertyChanges { target: root; status:  qsTr("Payment Sent") }
        },
        State {
            name: "rejected" // a peer didn't like our tx
            when: payment.broadcastStatus === Payment.TxRejected
            extend: "preparing"
            StateChangeScript { script: ControlColors.applyDarkSkin(root) }
            PropertyChanges { target: background; color: "#7f0000" }
            PropertyChanges { target: circleShape; opacity: 0 }
            PropertyChanges { target: root; status: qsTr("Failed") }
            PropertyChanges { target: errorLabel; text: qsTr("Transaction rejected by network") }
        }
    ]

    Rectangle {
        id: background
        width: parent.width
        height: parent.height
        opacity: 0
        visible: opacity > 0
        color: mainWindow.floweeGreen
        y: height + 2
        MouseArea {
            anchors.fill: parent // eat all mouse events.
        }

        Column {
            spacing: 10
            width: parent.width
            Label {
                id: errorLabel
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                width: parent.width
            }

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 160
                height: 160

                // The 'progress' icon.
                Shape {
                    id: circleShape
                    width: 160
                    height: width
                    smooth: true
                    ShapePath {
                        strokeWidth: 20
                        strokeColor: "#dedede"
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        startX: 100; startY: 10

                        PathAngleArc {
                            id: progressCircle
                            centerX: 80
                            centerY: 80
                            radiusX: 70; radiusY: 70
                            startAngle: -80
                            sweepAngle: 0
                            Behavior on sweepAngle {  NumberAnimation { duration: 2500 } }
                            Behavior on startAngle {  NumberAnimation { } }
                        }
                    }
                    Behavior on opacity {  NumberAnimation { } }
                }
                Shape {
                    id: checkShape
                    anchors.fill: circleShape
                    smooth: true
                    opacity: 0
                    ShapePath {
                        id: checkPath
                        strokeWidth: 16
                        strokeColor: "green"
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        startX: 52; startY: 80
                        PathLine { x: 76; y: 110 }
                        PathLine { x: 125; y: 47 }
                    }
                }
            }

            Label {
                id: fiatAmount
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: paymentAddressLabel.font.pixelSize * 1.5
                text: Fiat.formattedPrice(payment.paymentAmountFiat)
                visible: Fiat.price !== 0
            }
            BitcoinAmountLabel {
                id: cryptoAmount
                anchors.horizontalCenter: parent.horizontalCenter
                fontPixelSize: paymentAddressLabel.font.pixelSize * 1.15
                value: payment.paymentAmount
                colorize: false
                showFiat: false
            }

            Column {
                // column to avoid spacing between these two labels.
                width: parent.width
                visible: addressLabel.visible
                Label {
                    id: paymentAddressLabel
                    text: qsTr("Payment has been sent to:")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    width: parent.width
                }
                Label {
                    id: addressLabel
                    text: {
                        var answer = "";
                        for (let detail of payment.details) {
                            if (detail.isOutput) {
                                if (answer !== "") // then there are multiple outputs!
                                    return "";
                                answer = detail.niceAddress;
                            }
                        }
                        return answer;
                    }
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    width: parent.width
                    font.pixelSize: paymentAddressLabel.font.pixelSize * 0.8
                }
            }
            Item { width: 1; height: 10} // spacer
            RowLayout {
                id: txIdFeedback
                spacing: 30
                visible: root.state === "waiting" || root.state === "success"
                anchors.horizontalCenter: parent.horizontalCenter
                ImageButton {
                    source: "qrc:/edit-copy.svg"
                    onClicked: Pay.copyToClipboard(payment.formattedTargetAddress);
                    responseText: qsTr("Copied address to clipboard")
                }
                ImageButton {
                    source: "qrc:/internet.svg"
                    onClicked: Pay.openInExplorer(payment.txid);
                    responseText: qsTr("Opening Website")
                }
            }

            Item { width: 1; height: 10} // spacer

            TextField {
                id: transactionComment
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(400, parent.width - 40);
                onEditingFinished: payment.userComment = text
                placeholderText: qsTr("Add a personal note")
                placeholderTextColor: "#505050"
                text: payment.userComment
            }
        }
        Button {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 30
            anchors.right: parent.right
            anchors.rightMargin: 20
            text: qsTr("Close")
            onClicked: {
                payment.reset()
                transactionComment.text = ""
                root.closeButtonPressed();
            }
        }

        Behavior on opacity { NumberAnimation { } }
        Behavior on y { NumberAnimation { } }
        Behavior on color { ColorAnimation { } }
    }
}
