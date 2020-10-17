/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

Item {
    id: walletTransaction
    height: mainLabel.height + 10 + date.height
    width: mainLabel.width + bitcoinAmountLabel.width + 30

    /*
       we have
       model.fundsIn     the amount of satoshis consumed by inputs we own
       model.fundsOut    the amout of satoshis locked up in outputs we own

       model.txid
       model.height
       model.date
     */

    Label {
        id: mainLabel
        text: {
            if (model.fundsIn === 0)
                return qsTr("Received")
            let diff = model.fundsOut - model.fundsIn;
            if (diff > 0 && diff < 1000) // then the diff is likely just fees.
                return qsTr("Moved");
            return qsTr("Sent")
        }
    }
    Label {
        id: date
        anchors.top: mainLabel.bottom
        text: model.date
        opacity: 0.5
        font.pointSize: mainLabel.font.pointSize * 0.8
    }

    BitcoinAmountLabel {
        id: bitcoinAmountLabel
        value: model.fundsOut - model.fundsIn
        anchors.top: mainLabel.top
        anchors.right: parent.right
    }

    Rectangle {
        width: parent.width / 100 * 80
        height: 1
        color: "#99999B"
        opacity: 0.5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
    }
}
