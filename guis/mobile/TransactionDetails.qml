/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2024 Tom Zander <tom@flowee.org>
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

Page {
    id: root
    property QtObject transaction: null
    property QtObject infoObject: null
    headerText: qsTr("Transaction Details")

    property QtObject openInExplorer: QQC2.Action {
        text: qsTr("Open in Explorer")
        onTriggered: Pay.openInExplorer(root.transaction.txid);
    }
    menuItems: [ openInExplorer ]

    Flickable {
        anchors.fill: parent
        contentHeight: content.height
        ColumnLayout {
            id: content
            width: parent.width
            spacing: 10

            PageTitledBox {
                title: qsTr("Transaction Hash")

                Item {
                    implicitHeight: txidLabel.implicitHeight
                    width: parent.width

                    Flowee.Label {
                        id: txidLabel
                        text: root.transaction == null ? "" : root.transaction.txid
                        anchors.left: parent.left
                        anchors.right: copyIcon.left
                        anchors.rightMargin: 10
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: root.font.pixelSize * 0.9

                        Rectangle {
                            id: txidHighlight
                            anchors.fill: parent
                            color: palette.mid
                            opacity: 0
                            visible: opacity > 0
                            Timer {
                                running: parent.visible
                                onTriggered: parent.opacity = 0;
                                interval: 500
                            }
                            Behavior on opacity { NumberAnimation { duration: 250 } }
                        }
                    }
                    Image {
                        id: copyIcon
                        anchors.right: parent.right
                        width: 20
                        height: 20
                        source: "qrc:/edit-copy" + (Pay.useDarkSkin ? "-light" : "") + ".svg"
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -15
                            onClicked: {
                                txidHighlight.opacity = 0.6
                                Pay.copyToClipboard(txidLabel.text)
                            }
                        }
                    }
                }
            }
            PageTitledBox {
                title: qsTr("First Seen")
                Flowee.Label {
                    Layout.fillWidth: true
                    text: {
                        var tx = root.transaction; // delayed initialized
                        if (tx == null)
                            return "";
                        return Qt.formatDateTime(tx.date);
                    }
                }
            }

            PageTitledBox {
                title: {
                    if (root.transaction == null)
                        return ""
                    var h = root.transaction.height;
                    if (h === -2)
                        return qsTr("Rejected")
                    if (h === -1)
                        return qsTr("Unconfirmed")
                    return qsTr("Mined at")
                }
                Flowee.Label {
                    visible: text !== ""
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    Layout.fillWidth: true
                    text: {
                        var tx = root.transaction;
                        if (tx == null)
                            return "";
                        if (root.transaction.height < 1)
                            return "";

                        let txHeight = tx.height;
                        var answer = txHeight + "\n" + Pay.formatBlockTime(tx.height)
                        let blockAge = Pay.chainHeight - txHeight + 1;
                        answer += "\n";
                        answer += qsTr("%1 blocks ago", "", blockAge).arg(blockAge);
                        return answer;
                    }
                }
            }

            PageTitledBox {
                title: qsTr("Transaction comment")

                EditableLabel {
                    id: editableLabel
                    width: parent.width
                    text: root.transaction == null ? "" : root.transaction.comment
                    editable: root.infoObject != null && infoObject.commentEditable
                    onEdited: if (root.infoObject != null) infoObject.userComment = text
                }
            }

            PageTitledBox {
                id: fiatBox
                property double amountBch: {
                    if (root.transaction === null)
                        return 0;
                    if (isMoved) 
                        return root.transaction.fundsIn
                    return root.transaction.fundsOut - root.transaction.fundsIn;
                }
                // Is this transaction a 'move between addresses' tx.
                // This is a heuristic and not available in the model, which is why its in the view.
                property bool isMoved: {
                    if (root.transaction == null)
                        return false;
                    if (root.transaction.isCoinbase || root.transaction.isFused
                            || root.transaction.fundsIn === 0)
                        return false;
                    var amount = root.transaction.fundsOut - root.transaction.fundsIn
                    return amount < 0 && amount > -2500 // then the diff is likely just fees.
                }
                visible: {
                    if (root.transaction === null)
                        return false;
                    if (root.transaction.minedHeight < 1)
                        return false;
                    if (root.transaction.isFused)
                        return false;
                    if (isMoved)
                        return false;
                    if (valueThenLabel.fiatPrice === 0)
                        return false;
                    if (Math.abs(amountBch) < 10000) // hardcode 10k sats here, may need adjustment later
                        return false;
                    return true;
                }
                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    width: parent.width

                    Flowee.Label {
                        text: qsTr("Value now") + ":"
                    }
                    Flowee.Label {
                        text: {
                            if (root.transaction === null || root.transaction.minedHeight <= 0)
                                return "";
                            var fiatPriceNow = Fiat.price;
                            var gained = (fiatPriceNow - valueThenLabel.fiatPrice) / valueThenLabel.fiatPrice * 100
                            return Fiat.formattedPrice(Math.abs(fiatBox.amountBch), fiatPriceNow)
                                    + " (" + (gained >= 0 ? "↑" : "↓") + Math.abs(gained).toFixed(2) + "%)";
                        }
                    }

                    // price at mining
                    // value in exchange gained
                    Flowee.Label {
                        id: priceAtMining
                        text: qsTr("Value then") + ":"
                    }
                    Flowee.Label {
                        Layout.fillWidth: true
                        id: valueThenLabel
                        // when the backend does NOT get an 'accurate' (timewise) value,
                        // it returns zero. Which makes us set visibility to false
                        property int fiatPrice: {
                            if (root.transaction == null)
                                return 0;
                            Fiat.historicalPriceAccurate(root.transaction.date)
                        }
                        text: Fiat.formattedPrice(Math.abs(fiatBox.amountBch), fiatPrice)
                    }
                }
            }

            PageTitledBox {
                Flowee.Label {
                    text: root.infoObject == null ? "" : qsTr("Size: %1 bytes").arg(infoObject.size)
                }
                Flowee.Label {
                    text: qsTr("Coinbase")
                    visible: root.transaction != null && root.transaction.isCoinbase
                }
            }

            // We can't calculate the fees of just any transaction, only for the ones
            // this account created.
            PageTitledBox {
                title: qsTr("Fees paid")
                visible: root.infoObject != null && infoObject.createdByUs
                Flowee.Label {
                    text: root.infoObject == null ? "" : qsTr("%1 Satoshi / 1000 bytes")
                            .arg((infoObject.fees * 1000 / infoObject.size).toFixed(0))
                }
                Flowee.BitcoinAmountLabel {
                    value: root.infoObject == null ? 0 : infoObject.fees
                    fiatTimestamp: model.date
                    colorize: false
                }
            }

            PageTitledBox {
                spacing: 10
                title: {
                    if (root.infoObject == null)
                        return "";
                    if (infoObject.isFused)
                        return qsTr("Fused from my addresses");
                    if (infoObject.createdByUs)
                        return qsTr("Sent from my addresses");
                    if (infoObject.isFused)
                        return qsTr("Sent from addresses");
                    return "";
                }
                visible: title !== ""

                Repeater {
                    /*
                     * We only have the one transaction, which means we only have the previous txid and we have
                     * no details about which address or anything else these funds come from if we didn't
                     * create the Tx. So just don't show anything.
                     */
                    model: parent.visible ? infoObject.knownInputs : 0
                    delegate: Item {
                        Layout.alignment: Qt.AlignRight
                        width: content.width
                        height: {
                            if (modelData === null)
                                return 0;
                            if (inAddress.implicitWidth + 10 + amount.implicitWidth < content.width)
                                return inAddress.height + 10;
                            return inAddress.height + amount.height + 16;
                        }
                        Flowee.CFIcon {
                            id: fusedIcon
                            visible: modelData.fromFused
                        }
                        Flowee.AddressLabel {
                            id: inAddress
                            txInfo: modelData
                            x: fusedIcon.visible ? fusedIcon.width + 6 : 0
                            width: Math.min(implicitWidth, parent.width - (fusedIcon.visible ? fusedIcon.width: 0))
                        }
                        Flowee.BitcoinAmountLabel {
                            id: amount
                            visible: modelData !== null
                            value: modelData === null ? 0 : (-1 * modelData.value)
                            fiatTimestamp: root.transaction.date
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 10
                            font.pixelSize: root.font.pixelSize * 0.9
                        }
                    }
                }
            }

            PageTitledBox {
                spacing: 10
                title: {
                    if (root.infoObject == null)
                        return "";
                    if (infoObject.isFused)
                        return qsTr("Fused into my addresses");
                    if (infoObject.createdByUs)
                        return qsTr("Received at addresses");
                    return qsTr("Received at my addresses");
                }

                Repeater {
                    model: root.infoObject == null ? 0 : infoObject.knownOutputs
                    delegate: Item {
                        Layout.alignment: Qt.AlignRight
                        width: content.width
                        height: {
                            if (outAddress.implicitWidth + 10 + outAmount.implicitWidth < width)
                                return outAmount.height + 10;
                            return outAddress.height + outAmount.height + 16;
                        }
                        Flowee.AddressLabel {
                            id: outAddress
                            txInfo: modelData
                            highlight: modelData.forMe
                            width: Math.min(implicitWidth, parent.width)
                        }

                        Flowee.BitcoinAmountLabel {
                            id: outAmount
                            value: modelData.value
                            fiatTimestamp: root.transaction.date
                            colorize: modelData.forMe
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 10
                            font.pixelSize: root.font.pixelSize * 0.9
                        }
                    }
                }
            }
        }
    }
}
