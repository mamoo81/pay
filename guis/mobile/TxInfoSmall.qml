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

/*
 * This is a simple widget that is used in the AccountHistory page.
 * Notice that to work it expects in the parent context to be available several things,
 * among others the isMoved and amountBch values for the transaction it is displaying
 */

GridLayout {
    id: root
    columns: 2
    rowSpacing: 10
    // set by the parent page
    property QtObject infoObject: null

    property int minedHeight: model.height // local cache

    QQC2.Label {
        Layout.columnSpan: 2

        property bool isRejected: root.minedHeight == -2; // -2 is the magic block-height indicating 'rejected'
        text: {
            if (isRejected)
                return qsTr("Transaction is rejected")
            if (typeof root.minedHeight < 1)
                return qsTr("Processing")
            return "";
        }
        visible: text !== ""
        color: {
            if (isRejected) {
                // Transaction is rejected by network
                return Pay.useDarkSkin ? "#ec2327" : "#b41214";
            }
            return palette.windowText
        }
    }

    Flowee.Label {
        visible: root.minedHeight > 0
        text: qsTr("Mined") + ":"
    }
    Flowee.Label {
        Layout.fillWidth: true
        visible: root.minedHeight > 0
        text: {
            if (root.minedHeight <= 0)
                return "";
            var rc = Pay.formatDateTime(model.date);
            var confirmations = Pay.headerChainHeight - root.minedHeight + 1;
            if (confirmations > 0 && confirmations < 100)
                rc += " (" + qsTr("%1 blocks ago", "Confirmations", confirmations).arg(confirmations) + ")";
            return rc;
        }
    }
    Flowee.Label {
        id: paymentTypeLabel
        Layout.columnSpan: isMoved ? 2 : 1
        text: {
            if (model.isCoinbase)
                return qsTr("Miner Reward") + ":";
            if (model.isCashFusion)
                return qsTr("Cash Fusion") + ":";
            if (model.fundsIn === 0)
                return qsTr("Received") + ":";
            if (isMoved)
                return qsTr("Payment to self");
            return qsTr("Sent") + ":";
        }
    }
    Flowee.BitcoinAmountLabel {
        visible: isMoved === false
        Layout.fillWidth: true
        value: model.fundsOut - model.fundsIn + (infoObject == null ? 0 : infoObject.fees)
        fiatTimestamp: model.date
        showFiat: false // might not fit
    }

    // price at mining
    // value in exchange gained
    Flowee.Label {
        id: priceAtMining
        visible: {
            if (root.minedHeight < 1)
                return false;
            if (model.isCashFusion)
                return false;
            if (isMoved)
                return false;
            if (valueThenLabel.fiatPrice === 0)
                return false;
            return true;
        }
        text: qsTr("Value then") + ":"
    }
    Flowee.Label {
        Layout.fillWidth: true
        id: valueThenLabel
        visible: priceAtMining.visible
        // when the backend does NOT get an 'accurate' (timewise) value, it returns zero. Which makes us set visibility to false
        property int fiatPrice: Fiat.historicalPriceAccurate(model.date)
        text: Fiat.formattedPrice(Math.abs(amountBch), fiatPrice)
    }
    Flowee.Label {
        visible: priceAtMining.visible
        text: qsTr("Value now") + ":"
    }
    Flowee.Label {
        Layout.fillWidth: true
        visible: priceAtMining.visible
        text: {
            if (root.minedHeight <= 0)
                return "";
            var fiatPriceNow = Fiat.price;
            var gained = (fiatPriceNow - valueThenLabel.fiatPrice) / valueThenLabel.fiatPrice * 100
            return Fiat.formattedPrice(Math.abs(amountBch), fiatPriceNow)
                    + " (" + (gained >= 0 ? "↑" : "↓") + Math.abs(gained).toFixed(2) + "%)";
        }
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
