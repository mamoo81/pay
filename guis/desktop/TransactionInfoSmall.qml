/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2024 Tom Zander <tom@flowee.org>
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

/*
 * Mined:
 * Sent: / Fees:
 * Value now:
 * Value then:
 * TxId:
 * More details
 */

Item {
    id: root
    width: parent.width
    height: column.height + 3

    property QtObject infoObject: null
    property int minedHeight: model.height // local cache
    property bool isRejected: root.minedHeight == -2; // -2 is the magic block-height indicating 'rejected'
    property bool isMoved: {
        if (model.isCoinbase || model.isFused || model.fundsIn === 0)
            return false;
        var amount = model.fundsOut - model.fundsIn
        return amount < 0 && amount > -2500 // then the diff is likely just fees.
    }
    property double amountBch: isMoved ? model.fundsIn
                                       : (model.fundsOut - model.fundsIn)

    ColumnLayout {
        id: column
        width: parent.width
        Flowee.Label {
            id: rejectedLabel
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
            width: parent.width
            columns: 2

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

            // value line
            Flowee.Label {
                id: paymentTypeLabel
                visible: {
                    if (isMoved)
                        return false;
                    if (Pay.activityShowsBch) // avoid just duplicating data from the main delegate
                        return false;
                    return true;
                }
                text: {
                    if (model.isFused)
                        return qsTr("Fees") + ":";
                    return mainLabel.text + ":"
                }
            }
            Flowee.BitcoinAmountLabel {
                visible: paymentTypeLabel.visible
                colorizeValue: amountBch + (infoObject == null ? 0 : infoObject.fees)
                value: Math.abs(colorizeValue)
                fiatTimestamp: model.date
            }
            // txid line
            Flowee.Label {
                text: "TXID:"
            }
            Flowee.LabelWithClipboard {
                menuText: qsTr("Copy transaction-ID")
                text: model.txid
                font.pixelSize: mainLabel.font.pixelSize * 0.9
            }
        }

        Image {
            sourceSize.width: 22
            sourceSize.height: 22
            smooth: true
            visible: {
                if (root.infoObject == null)
                    return false;
                // visible if at least one output has a token.
                var outputs = root.infoObject.knownOutputs;
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

        GridLayout {
            width: parent.width
            columns: 2
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
    }
        Rectangle {
        width: parent.width * 0.7
        height: 2
        color: palette.midlight
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
    }
    Rectangle {
        color: "yellow"
        opacity: 0.5
        anchors.right: parent.right
        anchors.rightMargin: 20
        radius: 5
        width: 30
        height: 30
        MouseArea {
            anchors.fill: parent
            anchors.margins: -7
            onClicked: txDetailsWindow.openTab(model.walletIndex);
        }
    }
}
