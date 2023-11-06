/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2023 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee

Rectangle {
    id: txRoot
    height: {
        var rc = mainLabel.height + 10 + date.height
        if (detailsPane.item != null)
            rc += detailsPane.item.height + 10;
        return rc;
    }
    width: mainLabel.width + bitcoinAmountLabel.width + 30
    color: (index % 2) == 0 ? palette.light : palette.alternateBase

    property bool isRejected: model.height === -2 // -2 is the magic block-height indicating 'rejected'

    /*
       we have
       model.fundsIn     the amount of satoshis consumed by inputs we own
       model.fundsOut    the amout of satoshis locked up in outputs we own

       model.txid
       model.height
       model.date
       model.isCoinbase
       model.isFused
       model.comment
     */

    // overlay that indicates the transactions is new since last sync.
    Rectangle {
        id: isNewIndicator
        anchors.verticalCenter: mainLabel.verticalCenter
        width: model.isNew ? 5 : 0 // avoid taking space
        height: width
        radius: width
        color: Pay.useDarkSkin ? mainWindow.floweeSalmon : mainWindow.floweeBlue
    }
    Label {
        id: mainLabel
        anchors.left: isNewIndicator.right
        anchors.leftMargin: model.isNew ? 3 : 0
        text: {
            if (model.isCoinbase)
                return qsTr("Miner Reward")
            if (model.isFused)
                return qsTr("Fused")
            if (model.fundsIn === 0)
                return qsTr("Received")
            let diff = model.fundsOut - model.fundsIn;
            if (diff < 0 && diff > -1000) // then the diff is likely just fees.
                return qsTr("Moved");
            return qsTr("Sent")
        }
    }
    Label {
        id: date
        anchors.top: mainLabel.bottom
        function updateText() {
            if (txRoot.isRejected)
                text = qsTr("rejected")
            var dat = model.date;
            if (typeof dat === "undefined")
                text = qsTr("unconfirmed")
            else
                text = Pay.formatDateTime(dat);
        }
        opacity: txRoot.isRejected ? 1 : 0.5
        font.pointSize: mainLabel.font.pointSize * 0.8
        color: txRoot.isRejected ? (Pay.useDarkSkin ? "#ec2327" : "#b41214") : palette.windowText

        Component.onCompleted: updateText()
        Timer {
            interval: 60000
            running: !txRoot.isRejected
            repeat: true
            onTriggered: parent.updateText()
        }
    }

    Flowee.CFIcon {
        id: fusedIcon
        visible: model.isFused
        anchors.right: userComment.left
        anchors.rightMargin: 6
        anchors.verticalCenter: userComment.verticalCenter
        width: 16
        height: 16
    }

    Label {
        id: userComment
        y: (date.y + date.height - height) / 2

        anchors {
            // Pick the widest label to be aligned to
            left: date.width > mainLabel.width ? date.right : mainLabel.right
            right: bitcoinAmountLabel.left
            leftMargin: fusedIcon.visible ? 25 : 6
            rightMargin: 10
        }
        text: model.comment
        elide: Text.ElideRight
        Connections {
            target: date
            function onTextChanged() {
                if (date.width > mainLabel.width) {
                    userComment.anchors.left =  date.right
                } else {
                    userComment.anchors.left =  mainLabel.right
                }
            }
        }
    }

    Flowee.BitcoinAmountLabel {
        id: bitcoinAmountLabel
        visible: Pay.activityShowsBch || !Pay.isMainChain
        value: {
            let inputs = model.fundsIn
            let outputs = model.fundsOut
            let diff = model.fundsOut - model.fundsIn
            if (!model.isFused
                    && diff < 0 && diff > -1000) // then the diff is likely just fees.
                return inputs;
            return diff;
        }
        anchors.top: mainLabel.top
        anchors.right: parent.right
    }
    Flowee.Label {
        anchors.top: mainLabel.top
        anchors.right: parent.right
        visible: bitcoinAmountLabel.visible === false
        text: {
            var timestamp = model.date;
            if (timestamp === undefined)
                var fiatPrice = Fiat.price; // todays price
            else
                fiatPrice = Fiat.historicalPrice(timestamp);
            return Fiat.formattedPrice(bitcoinAmountLabel.value, fiatPrice)
        }
        color: {
            var num = bitcoinAmountLabel.value
            if (num > 0) // positive value
                return Pay.useDarkSkin ? "#86ffa8" : "green";
            else if (num < 0) // negative
                return Pay.useDarkSkin ? "#ffdede" : "#444446";
            // zero is shown without normally
            return palette.windowText
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: detailsPane.source = (detailsPane.source == "") ? "./WalletTransactionDetails.qml" : ""
    }

    Loader {
        id: detailsPane
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        x: 4 // indent it
        width: parent.width - 6
        onLoaded: item.infoObject = portfolio.current.txInfo(model.walletIndex, item)
    }

    Behavior on height { NumberAnimation { duration: 100 } }
}
