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

ColumnLayout {
    id: root
    // set by the parent page
    property QtObject infoObject: null
    property int minedHeight: model.height // local cache

    Flowee.Label {
        property bool isRejected: root.minedHeight == -2; // -2 is the magic block-height indicating 'rejected'
        text: {
            if (isRejected)
                return qsTr("Transaction is rejected")
            if (typeof root.minedHeight < 1)
                return qsTr("Processing")
            return "";
        }
        visible: text !== ""
        color: isRejected ? mainWindow.errorRed : palette.windowText
    }

    GridLayout {
        columns: 2
        width: parent.width

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
                var rc = Pay.formatBlockTime(root.minedHeight);
                var confirmations = Pay.headerChainHeight - root.minedHeight + 1;
                if (confirmations > 0 && confirmations < 20)
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
                if (model.isFused)
                    return qsTr("Fees") + ":";
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
            colorizeValue: model.fundsOut - model.fundsIn + (infoObject == null ? 0 : infoObject.fees)
            value: Math.abs(colorizeValue)
            fiatTimestamp: model.date
            showFiat: false // might not fit
        }
    }

    Image {
        sourceSize.width: 22
        sourceSize.height: 22
        smooth: true
        visible: {
            if (infoObject == null)
                return false;
            // visible if at least one output has a token.
            var outputs = infoObject.knownOutputs;
            for (let o of outputs) {
                if (o !== null && o.hasCashToken)
                    return true;
            }
            return false;
        }
        source: visible ? "qrc:/CashTokens.svg" : ""
        Flowee.Label {
            x: 30
            text: qsTr("Holds a token")
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Flowee.Label {
        text: qsTr("Sent to") + ":"
        visible: receiverName.text !== ""
    }
    Flowee.LabelWithClipboard {
        id: receiverName
        Layout.alignment: Qt.AlignRight
        visible: text !== ""
        text: infoObject == null ? "" : infoObject.receiver
        font.pixelSize: paymentTypeLabel.font.pixelSize * 0.8
    }
    GridLayout {
        columns: 2
        rowSpacing: 10
        width: parent.width

        Flowee.Label {
            visible: priceAtMining.visible
            text: qsTr("Value now") + ":"
        }
        Flowee.Label {
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

        // price at mining
        // value in exchange gained
        Flowee.Label {
            id: priceAtMining
            visible: {
                if (root.minedHeight < 1)
                    return false;
                if (model.isFused)
                    return false;
                if (isMoved)
                    return false;
                if (valueThenLabel.fiatPrice === 0)
                    return false;
                if (Math.abs(amountBch) < 10000) // hardcode 10k sats here, may need adjustment later
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
    }

    TextButton {
        id: txDetailsButton
        text: qsTr("Transaction Details")
        showPageIcon: true
        onClicked: {
            var newItem = thePile.push("./TransactionDetails.qml")
            popupOverlay.close();
            newItem.transaction = model;
            newItem.infoObject = root.infoObject;
        }
    }
    Item { width: 10; height: 4 } // spacing
}
