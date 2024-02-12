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
import Flowee.org.pay

QQC2.ApplicationWindow {
    id: root
    visible: false
    minimumWidth: 200
    minimumHeight: 200
    width: 400
    height: 500
    title: qsTr("Transaction Details")
    modality: Qt.NonModal
    flags: Qt.Widget

    function openTab(walletIndex) {
        transactions.push(portfolio.current.txInfo(walletIndex, root))
        tabList.model = transactions.length
        // open the newly created tab
        tabbar.currentIndex = transactions.length - 1
    }
    property var transactions: []

    Rectangle {
        width: parent.width
        height: tabbar.headerHeight
        color: Pay.useDarkSkin ? palette.window : mainWindow.floweeBlue
        Rectangle {
            anchors.fill: parent
            opacity: 0.2
            color: Pay.useDarkSkin ? "black" : "white"
        }
    }

    Flowee.TabBar {
        id: tabbar
        anchors.fill: parent

        Repeater {
            id: tabList
            delegate: Flickable {
                id: delegateRoot
                anchors.fill: parent
                contentHeight: content.height + 20
                clip: true

                property QtObject txInfo: root.transactions[index];
                // properties for the tabbar
                property string title: txInfo.txid
                property string icon: ""

                Flowee.CloseIcon {
                    anchors.right: parent.right
                    y: 6
                    anchors.rightMargin: 6
                    onClicked: {
                        root.transactions.splice(index, 1);
                        if (root.transactions.length == 0) {
                            root.close();
                        }
                        else {
                            tabbar.currentIndex = Math.max(0, index - 1)
                            tabList.model = transactions.length
                        }
                    }
                }

                Column {
                    id: content
                    width: parent.width - 20
                    x: 10
                    y: 10
                    spacing: 10

                    GridLayout {
                        width: parent.width
                        columns: 2

                        Flowee.Label {
                            text: qsTr("First Seen") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.Label {
                            Layout.fillWidth: true
                            text: Qt.formatDateTime(txInfo.date)
                        }

                        Flowee.Label {
                            visible: minedDateLabel.visible
                            Layout.alignment: Qt.AlignTop | Qt.AlignRight
                            text: {
                                var h = txInfo.height;
                                if (h === -2)
                                    var s = qsTr("Rejected")
                                if (h === -1)
                                    s = qsTr("Unconfirmed")
                                else
                                    s = qsTr("Mined at");
                                return s + ":";
                            }
                        }
                        Flowee.Label {
                            id: minedDateLabel
                            visible: text !== ""
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            Layout.fillWidth: true
                            text: {
                                let txHeight = txInfo.minedHeight;
                                if (txHeight < 1)
                                    return "";

                                var answer = txHeight + "\n" + Pay.formatBlockTime(txHeight)
                                    let blockAge = Pay.chainHeight - txHeight + 1;
                                answer += "\n";
                                answer += qsTr("%1 blocks ago", "", blockAge).arg(blockAge);
                                return answer;
                            }
                        }

                        Flowee.Label {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("Comment") + ":"
                        }
                        Flowee.TextField {
                            id: editableLabel
                            Layout.fillWidth: true
                            text: txInfo.userComment
                            onTextChanged: txInfo.userComment = text
                        }
                        Flowee.FiatTxInfo {
                            txInfo: delegateRoot.txInfo
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                        }
                        // We can't calculate the fees of just any transaction,
                        // only for the ones this account created.
                        Flowee.Label {
                            id: feesSection
                            text: qsTr("Fees paid") + ":"
                            visible: txInfo.createdByUs
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.Label {
                            visible: feesSection.visible
                            text: qsTr("%1 Satoshi / 1000 bytes")
                                .arg((txInfo.fees * 1000 / txInfo.size).toFixed(0))
                        }
                        Item {
                            visible: feesSection.visible
                            width: 1; height: 1
                        }
                        Flowee.BitcoinAmountLabel {
                            visible: feesSection.visible
                            value: txInfo.fees
                            fiatTimestamp: txInfo.date
                            colorize: false
                        }

                        Flowee.Label {
                            text: qsTr("Size") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.Label {
                            text: qsTr("%1 bytes").arg(txInfo.size)
                        }
                        Item {
                            width: 1; height: 1
                            visible: txInfo.isCoinbase
                        }
                        Flowee.Label {
                            text: qsTr("Is Coinbase")
                            visible: txInfo.isCoinbase
                        }

                        // txid line
                        Flowee.Label {
                            text: "TXID:"
                            Layout.alignment: Qt.AlignTop | Qt.AlignRight
                        }
                        Flowee.LabelWithClipboard {
                            menuText: qsTr("Copy transaction-ID")
                            text: txInfo.txid
                            font.pixelSize: root.font.pixelSize * 0.9
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            Layout.fillWidth: true
                        }
                    }

                    Flowee.Label {
                        id: inputsList
                        text: {
                            if (txInfo == null)
                                return "";
                            if (txInfo.isFused)
                                return qsTr("Fused from my addresses");
                            if (txInfo.createdByUs)
                                return qsTr("Sent from my addresses");
                            if (txInfo.isFused)
                                return qsTr("Sent from addresses");
                            return "";
                        }
                        visible: title !== ""
                        font.bold: true
                    }

                    Repeater {
                        model: inputsList.visible ? txInfo.knownInputs : 0
                        delegate: Item {
                            Layout.alignment: Qt.AlignRight
                            width: content.width
                            height: {
                                if (modelData === null)
                                    return 6;
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
                                fiatTimestamp: txInfo.date
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 10
                            }
                        }
                    }

                    Flowee.Label {
                        text: {
                            if (txInfo == null)
                                return "";
                            if (txInfo.isFused)
                                return qsTr("Fused into my addresses");
                            if (txInfo.createdByUs)
                                return qsTr("Received at addresses");
                            return qsTr("Received at my addresses");
                        }
                        font.bold: true
                    }

                    Repeater {
                        model: txInfo.knownOutputs
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
                                fiatTimestamp: txInfo.date
                                colorize: modelData.forMe
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 10
                            }
                        }
                    }
                }
            }
        }
    }
}
