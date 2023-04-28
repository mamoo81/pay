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
        if (request.state !== PaymentRequest.Unpaid) {
            console.log(" show dialog to request change of account");
        }
        else {
            request.account = account;
        }
    }
    onActiveFocusChanged: {
        if (activeFocus) {
            console.log("  startign");
            request.start();
        }
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

}
