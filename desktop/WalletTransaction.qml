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
import "widgets" as Flowee

Item {
    id: txRoot
    height: {
        var rc = mainLabel.height + 10 + date.height
        if (detailsPane.item != null)
            rc += detailsPane.item.height + 10;
        return rc;
    }
    width: mainLabel.width + bitcoinAmountLabel.width + 30

    property bool isRejected: model.height === -2 // -2 is the magic block-height indicating 'rejected'

    /*
       we have
       model.fundsIn     the amount of satoshis consumed by inputs we own
       model.fundsOut    the amout of satoshis locked up in outputs we own

       model.txid
       model.height
       model.date
       model.isCoinbase
       model.isCashFusion
       model.comment
     */

    // overlay that indicates the transactions is new since last sync.
    Rectangle {
        id: isNewIndicator
        anchors.verticalCenter: mainLabel.verticalCenter
        width: model.isNew ? mainLabel.height * 0.4 : 0
        height: width
        radius: height
        color: Pay.useDarkSkin ? mainWindow.floweeSalmon : mainWindow.floweeBlue
    }
    Label {
        id: mainLabel
        anchors.left: isNewIndicator.right
        anchors.leftMargin: model.isNew ? 10 : 0
        text: {
            if (model.isCoinbase)
                return qsTr("Miner Reward")
            if (model.isCashFusion)
                return qsTr("Cash Fusion")
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
        color: txRoot.isRejected ? (Pay.useDarkSkin ? "#ec2327" : "#b41214") : palette.text

        Component.onCompleted: updateText()
        Timer {
            interval: 60000
            running: !txRoot.isRejected
            repeat: true
            onTriggered: parent.updateText()
        }
    }

    Flowee.CashFusionIcon {
        id: fusedIcon
        visible: model.isCashFusion
        anchors.right: userComment.left
        anchors.rightMargin: 10
        anchors.verticalCenter: userComment.verticalCenter
    }

    Label {
        id: userComment
        y: (date.y + date.height - height) / 2

        anchors {
            // Pick the widest label to be aligned to
            left: date.width > mainLabel.width ? date.right : mainLabel.right
            right: bitcoinAmountLabel.left
            leftMargin: fusedIcon.visible ? 44 : 10
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
        value: {
            let inputs = model.fundsIn
            let outputs = model.fundsOut
            let diff = model.fundsOut - model.fundsIn
            if (!model.isCashFusion
                    && diff < 0 && diff > -1000) // then the diff is likely just fees.
                return inputs;
            return diff;
        }
        anchors.top: mainLabel.top
        fontPtSize: date.font.pointSize
        anchors.right: parent.right
    }

    // visual separator
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
