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

/* This frame shows a wallet-name, some basic info and the historical transactions */
Rectangle {
    id: root
    visible: opacity !== 0
    color: "#00000000" // transparant
    border.color: titleRow.border.color
    border.width: titleRow.border.width

    property QtObject wallet: walletDetails.active ? wallets.current : null

    Rectangle {
        id: titleRow
        width: parent.width
        height: titleLabel.height + 20
        color: "#eafeed"
        border.color: "black"
        border.width: 3
        Text {
            id: titleLabel
            text: root.wallet === null ? "" : root.wallet.name
            font.pixelSize: 26
            font.bold: true
            anchors.centerIn: parent
            wrapMode: Text.WordWrap
        }
    }

    BitcoinAmountLabel {
        x: 10
        id: balance
        anchors.top: titleRow.bottom
        value: root.wallet === null ? 0 : root.wallet.balance
        colorize: false
    }

    Text {
        id: utxoRow
        anchors.top: balance.bottom
        x: 10
        opacity: 0.8
        text: {
            var wallet = root.wallet
            if (wallet === null)
                return ""

            return qsTr("Unspent: %1, historical: %2").arg(wallet.unspentOutputCount)
                                                      .arg(wallet.historicalOutputCount);
        }
    }
    ListView {
        clip: true
        anchors.top: utxoRow.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 10
        model: root.wallet.transactions

        delegate: WalletTransaction {
            // width: parent.width
        }
    }
}
