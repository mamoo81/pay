/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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
                            onClicked: Pay.copyToClipboard(txidLabel.text)
                        }
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
                        var answer = txHeight + "\n" + Pay.formatDateTime(tx.date)
                        let blockAge = Pay.chainHeight - txHeight;
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
                Flowee.Label {
                    text: root.infoObject == null ? "" : qsTr("Size: %1 bytes").arg(infoObject.size)
                }
                Flowee.Label {
                    text: qsTr("Coinbase")
                    visible: root.transaction != null && root.transaction.isCoinbase
                }
                Flowee.Label {
                    text: qsTr("Is a CashFusion transaction.")
                    visible: root.transaction != null && root.transaction.isCashFusion
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
                    colorize: false
                }
            }

            PageTitledBox {
                title: {
                    if (root.infoObject == null)
                        return "";
                    if (infoObject.isCashFusion)
                        return qsTr("Fused from my addresses");
                    if (infoObject.createdByUs)
                        return qsTr("Sent from my addresses");
                    if (infoObject.isCashFusion)
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
                    model: parent.visible ? infoObject.inputs : 0
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
                        Rectangle {
                            color: Pay.useDarkSkin ? "#4fb2e7" : "yellow"
                            visible: inAddress.visible
                            x: inAddress.x - 3
                            y: inAddress.y -3
                            height: inAddress.height + 6
                            width: Math.min(inAddress.width, inAddress.contentWidth) + 6
                            radius: height / 3
                            opacity: 0.2
                        }
                        Flowee.LabelWithClipboard {
                            id: inAddress
                            menuText: qsTr("Copy Address")
                            text: {
                                if (modelData === null)
                                    return "";
                                var cloaked = modelData.cloakedAddress
                                if (cloaked !== "")
                                    return cloaked;
                                return modelData.address;
                            }
                            width: parent.width
                            clipboardText: modelData === null ? "" : modelData.address
                            visible: modelData !== null
                            font.pixelSize: root.font.pixelSize * 0.9
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
                title: {
                    if (root.infoObject == null)
                        return "";
                    if (infoObject.isCashFusion)
                        return qsTr("Fused into my addresses");
                    if (infoObject.createdByUs)
                        return qsTr("Received at addresses");
                    return qsTr("Received at my addresses");
                }
                Repeater {
                    model: root.infoObject == null ? 0 : infoObject.outputs
                    delegate: Item {
                        Layout.alignment: Qt.AlignRight
                        width: content.width
                        height: {
                            if (modelData === null)
                                return 0;
                            if (outAddress.implicitWidth + 10 + outAmount.implicitWidth < width)
                                return outAmount.height + 10;
                            return outAddress.height + outAmount.height + 16;
                        }
                        Rectangle {
                            color: Pay.useDarkSkin ? "#4fb2e7" : "yellow"
                            visible: modelData !== null && modelData.forMe
                            x: outAddress.x - 3
                            y: outAddress.y -3
                            height: outAddress.height + 6
                            width: Math.min(outAddress.width, outAddress.contentWidth) + 6
                            radius: height / 3
                            opacity: 0.2
                        }
                        Flowee.LabelWithClipboard {
                            id: outAddress
                            visible: modelData !== null
                            elide: Text.ElideMiddle
                            menuText: qsTr("Copy Address")
                            text: {
                                if (modelData === null)
                                    return "";
                                var cloaked = modelData.cloakedAddress
                                if (cloaked !== "")
                                    return cloaked;
                                return modelData.address;
                            }
                            clipboardText: modelData === null ? "" : modelData.address
                            width: parent.width
                            font.pixelSize: root.font.pixelSize * 0.9
                        }
                        Flowee.BitcoinAmountLabel {
                            id: outAmount
                            visible: modelData !== null
                            value: modelData === null ? 0 : modelData.value
                            fiatTimestamp: root.transaction.date
                            colorize: modelData !== null && modelData.forMe
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
