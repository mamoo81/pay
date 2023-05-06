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
            Flowee.Label {
                text: qsTr("Transaction Hash") + ":"
            }
            Item {
                implicitHeight: txidLabel.implicitHeight
                Layout.fillWidth: true
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
            Flowee.Label {
                text: {
                    if (root.transaction == null)
                        return ""
                    var h = root.transaction.height;
                    if (h === -2)
                        return qsTr("Rejected")
                    if (h === -1)
                        return qsTr("Unconfirmed")
                    return qsTr("Mined") + ": "
                }
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

                    var answer = Pay.formatDateTime(tx.date)
                    answer += "\n";
                    let txHeight = tx.height;
                    let blockAge = Pay.chainHeight - txHeight;
                    answer += qsTr("%1 blocks ago", "", blockAge).arg(blockAge);

                    answer += "\n" + txHeight;
                    return answer;
                }
            }
            Flowee.Label {
                text: qsTr("Transaction comment") + ":"
            }
            Item {
                Layout.fillWidth: true
                implicitHeight: commentEdit.implicitHeight

                Flowee.Label {
                    id: commentLabel
                    text: root.transaction == null ? "" : root.transaction.comment
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    anchors.left: parent.left
                    anchors.right: editIcon.left
                    anchors.rightMargin: 10
                    anchors.baseline: commentEdit.baseline
                    visible: !commentEdit.visible
                }
                Image {
                    id: editIcon
                    anchors.right: parent.right
                    width: 20
                    height: 20
                    smooth: true
                    source: "qrc:/edit" + (Pay.useDarkSkin ? "-light" : "") + ".svg"
                    // Editing is a risky business until QTBUG-109306 has been deployed
                    // visible: root.infoObject != null && root.infoObject.commentEditable
                    visible: false
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -15
                        onClicked: {
                            commentEdit.visible = true
                            commentEdit.focus = true
                            commentEdit.forceActiveFocus();
                        }
                    }
                }
                Flowee.TextField {
                    id: commentEdit
                    anchors.left: parent.left
                    anchors.right: editIcon.left
                    anchors.rightMargin: 10
                    visible: false
                    text:  root.transaction == null ? "" : root.transaction.comment
                    onTextChanged: if (root.infoObject != null) root.infoObject.userComment = text
                }
            }
            Flowee.Label {
                text: qsTr("Size") + ":"
            }
            Flowee.Label {
                text: root.infoObject == null ? "" :
                        qsTr("%1 bytes").arg(root.infoObject.size)
            }

            Flowee.Label {
                text: qsTr("Coinbase")
                visible: root.transaction != null && root.transaction.isCoinbase
            }
            Flowee.Label {
                text: qsTr("CashFusion transaction")
                visible: root.transaction != null && root.transaction.isCashFusion
            }

            // We can't calculate the fees of just any transaction, only for the ones
            // this account created.
            Flowee.Label {
                id: feesPaidLabel
                visible: root.infoObject != null && root.infoObject.createdByUs
                text: qsTr("Fees paid") + ":"
            }
            Flowee.Label {
                visible: feesPaidLabel.visible
                text: root.infoObject == null ? "" : qsTr("%1 Satoshi / 1000 bytes")
                        .arg((root.infoObject.fees * 1000 / root.infoObject.size).toFixed(0))
            }
            Flowee.BitcoinAmountLabel {
                visible: feesPaidLabel.visible
                value: root.infoObject == null ? 0 : root.infoObject.fees
            }

            Flowee.Label { text: qsTr("Senders") + ":" }
            Repeater {
                model: root.infoObject == null ? 0 : root.infoObject.inputs
                delegate: Item {
                    Layout.alignment: Qt.AlignRight
                    width: content.width - 10
                    height: modelData === null ? 6 : (5 + inAddress.height + amount.height)
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
                        font.pixelSize: root.font.pixelSize * 0.8
                    }
                    Flowee.BitcoinAmountLabel {
                        id: amount
                        visible: modelData !== null
                        value: modelData === null ? 0 : (-1 * modelData.value)
                        fiatTimestamp: root.transaction.date
                        anchors.right: parent.right
                        anchors.top: inAddress.bottom
                    }
                }
            }

            Flowee.Label { text: qsTr("Receivers") + ":" }
            Repeater {
                model: root.infoObject == null ? 0 : root.infoObject.outputs
                delegate: Item {
                    Layout.alignment: Qt.AlignRight
                    width: content.width - 10
                    height: modelData === null ? 6 : (5 + outAddress.height + outAmount.height)
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
                        font.pixelSize: root.font.pixelSize * 0.8
                    }
                    Flowee.BitcoinAmountLabel {
                        id: outAmount
                        visible: modelData !== null
                        value: modelData === null ? 0 : modelData.value
                        fiatTimestamp: root.transaction.date
                        colorize: modelData !== null && modelData.forMe
                        anchors.right: parent.right
                        anchors.top: outAddress.bottom
                    }
                }
            }
        }
    }
}
