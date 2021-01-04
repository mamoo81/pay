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
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

import Flowee.org.pay 1.0

Pane {
    id: receivePane
    height: qrCode.height + grid.height

    property QtObject account: root.account
    property QtObject request: account.createPaymentRequest(receivePane)

    Image {
        id: qrCode
        width: 300
        height: 300
        source: "image://qr/" + receivePane.request.qr
        smooth: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 40

        MouseArea {
            anchors.fill: parent
            onClicked: {
                Flowee.copyToClipboard(receivePane.request.qr)
                // TODO show feedback
            }
        }
    }

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: qrCode.bottom
        anchors.topMargin: 20
        columns: 2
        Label {
            text: qsTr("Description:")
        }
        TextField {
            id: description
            Layout.fillWidth: true
            onTextChanged: receivePane.request.message = text
        }
        // next row
        Label {
            id: payAmount
            text: "Amount:"
        }
        BitcoinValueField {
            id: bitcoinValueField
            fontPtSize: payAmount.font.pointSize
            onValueChanged: receivePane.request.amount = value
        }
    }

    /*
      Destination address:
        - if this is a single address wallet, than this is simple.
        - have a right-click-on-address menu for everywhere else
          which has copy but also has "request payment to...".
          This address should be possible to show here too.

       Description
       Amount

       BIP70 style receiving
       New style receiving.
     */
}
