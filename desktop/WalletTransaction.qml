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

Item {
    height: {
        var rc = mainLabel.height + 10 + date.height
        if (detailsPane.item != null)
            rc += detailsPane.item.height + 10;
        return rc;
    }
    width: mainLabel.width + bitcoinAmountLabel.width + 30

    /*
       we have
       model.fundsIn     the amount of satoshis consumed by inputs we own
       model.fundsOut    the amout of satoshis locked up in outputs we own

       model.txid
       model.height
       model.date
     */

    // overlay that indicates the transactions is new since last sync.
    Rectangle {
        id: isNewIndicator
        anchors.verticalCenter: mainLabel.verticalCenter
        width: model.isNew ? mainLabel.height / 3 * 2 : 0
        height: width
        radius: height
        color: Pay.useDarkSkin ? mainWindow.floweeSalmon : mainWindow.floweeBlue
    }
    Label {
        id: mainLabel
        anchors.left: isNewIndicator.right
        anchors.leftMargin: model.isNew ? 10 : 0
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
        text: {
            var dat = model.date;
            if (typeof dat === "undefined")
                return qsTr("unconfirmed")
            return Pay.formatDateTime(dat);
        }
        opacity: 0.5
        font.pointSize: mainLabel.font.pointSize * 0.8
    }


    BitcoinAmountLabel {
        id: bitcoinAmountLabel
        value: model.fundsOut - model.fundsIn
        anchors.top: mainLabel.top
        fontPtSize: date.font.pointSize
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

    MouseArea {
        anchors.fill: parent
        onClicked: detailsPane.source = (detailsPane.source == "") ? "./WalletTransactionDetails.qml" : ""
    }

    Loader {
        id: detailsPane
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        x: 10 // indent it
        width: parent.width - 10
        onLoaded: item.infoObject = portfolio.current.txInfo(model.walletIndex, item)
    }

    Behavior on height { NumberAnimation { duration: 100 } }
}
