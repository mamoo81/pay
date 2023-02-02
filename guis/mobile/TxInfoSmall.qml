/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee
import Flowee.org.pay;

GridLayout {
    id: root
    columns: 2
    rowSpacing: 10
    property QtObject infoObject: null
    Flowee.LabelWithClipboard {
        Layout.fillWidth: true
        Layout.columnSpan: 2
        text: {
            if (model.height === -2)// -2 is the magic block-height indicating 'rejected'
                return qsTr("rejected")
            if (typeof model.date === "undefined")
                return qsTr("unconfirmed")
            var confirmations = Pay.headerChainHeight - model.height + 1;
            return qsTr("%1 confirmations (mined in block %2)", "", confirmations)
            .arg(confirmations).arg(model.height);
        }
        clipboardText: model.height
        menuText: qsTr("Copy block height")
    }
    Flowee.Label {
        visible: model.height > 0
        text: qsTr("Mined") + ":"
    }
    Flowee.Label {
        visible: model.height > 0
        text: model.height > 0 ? Pay.formatDateTime(model.date) : "";
    }
    Flowee.Label {
        id: paymentTypeLabel
        visible: {
            let diff = model.fundsOut - model.fundsIn;
            if (diff < 0 && diff > -1000) // this is our heuristic to mark it as 'moved'
                return false;
            return true;
        }
        text: {
            if (model.isCoinbase)
                return qsTr("Miner Reward") + ":";
            if (model.isCashFusion)
                return qsTr("Cash Fusion") + ":";
            if (model.fundsIn === 0)
                return qsTr("Received") + ":";
            let diff = model.fundsOut - model.fundsIn;
            if (diff < 0 && diff > -1000) // then the diff is likely just fees.
                return qsTr("Moved") + ":";
            return qsTr("Sent") + ":";
        }
    }
    Flowee.BitcoinAmountLabel {
        visible: paymentTypeLabel.visible
        value: model.fundsOut - model.fundsIn
        fiatTimestamp: model.date
    }
    Flowee.Label {
        id: feesLabel
        visible: root.infoObject != null && root.infoObject.createdByUs
        text: qsTr("Fees") + ":"
    }
    Flowee.BitcoinAmountLabel {
        visible: feesLabel.visible
        value: {
            if (root.infoObject == null)
                return 0;
            if (!root.infoObject.createdByUs)
                return 0;
            var amount = model.fundsIn;
            var outputs = root.infoObject.outputs
            for (var i in outputs) {
                amount -= outputs[i].value
            }
            return amount
        }
        fiatTimestamp: model.date
        colorize: false
    }

    TextButton {
        id: txDetailsButton
        Layout.columnSpan: 2
        text: qsTr("Transaction Details")
        showPageIcon: true
        onClicked: {
            var newItem = thePile.push("./TransactionDetails.qml")
            popupOverlay.close();
            newItem.transaction = model;
            newItem.infoObject = root.infoObject;
        }
    }
}
