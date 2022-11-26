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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Flowee" as Flowee

GridLayout {
    id: root

    property QtObject infoObject: null
    property var minedDate: model.date

    columns: 2
    Flowee.LabelWithClipboard {
        menuText: qsTr("Copy transaction-ID")
        Layout.columnSpan: 2
        text: model.txid
        font.pixelSize: 12
    }
    Label {
        text: qsTr("Status") + ":"
    }
    Flowee.LabelWithClipboard {
        id: rightColumn
        Layout.fillWidth: true
        text: {
            if (txRoot.isRejected)
                return qsTr("rejected")
            if (typeof root.minedDate === "undefined")
                return qsTr("unconfirmed")
            var confirmations = Pay.headerChainHeight - model.height + 1;
            return qsTr("%1 confirmations (mined in block %2)", "", confirmations)
                .arg(confirmations).arg(model.height);
        }
        clipboardText: model.height
        menuText: qsTr("Copy block height")
    }

    Label {
        id: paymentTypeLabel
        visible: {
            let diff = model.fundsOut - model.fundsIn;
            if (diff < 0 && diff > -1000) // this is our heuristic to mark it as 'moved'
                return false;
            return true;
        }
        text: mainLabel.text + ":"
    }
    Flowee.BitcoinAmountLabel {
        visible: paymentTypeLabel.visible
        value: model.fundsOut - model.fundsIn
        fiatTimestamp: root.minedDate
    }
    Label {
        id: feesLabel
        visible: infoObject.createdByUs
        text: qsTr("Fees") + ":"
    }
    Flowee.BitcoinAmountLabel {
        visible: feesLabel.visible
        value: {
            if (!infoObject.createdByUs)
                return 0;
            var amount = model.fundsIn;
            var outputs = infoObject.outputs
            for (var i in outputs) {
                amount -= outputs[i].value
            }
            return amount
        }
        fiatTimestamp: root.minedDate
        colorize: false
    }
    Label {
        text: qsTr("Size") + ":"
    }
    Label {
        text: qsTr("%1 bytes", "", infoObject.size).arg(infoObject.size)
    }
    Label {
        text: qsTr("Inputs") + ":"
        Layout.alignment: Qt.AlignTop
        visible: infoObject.inputs.length > 0
    }
    ColumnLayout {
        Layout.fillWidth: true
        Repeater {
            model: infoObject.inputs
            delegate:
                Item {
                    width: rightColumn.width
                    height: modelData === null ? 6 : (5 + inAddress.height)
                    Flowee.ArrowPoint {
                        id: arrowPoint
                        anchors.bottom: parent.bottom
                        color: arrowLine.color
                    }
                    Rectangle {
                        id: arrowLine
                        color: modelData === null ? "grey" : "#c5c537"
                        anchors.left: arrowPoint.right
                        anchors.leftMargin: -2 // overlap the line and the arrow
                        anchors.bottom: arrowPoint.bottom
                        anchors.bottomMargin: 6
                        anchors.right: parent.horizontalCenter
                        height: 1.6
                    }
                    Label {
                        id: inIndex
                        text: index
                        visible: modelData !== null
                        anchors.left: arrowPoint.right
                        anchors.leftMargin: 6
                        anchors.bottom: arrowLine.top
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
                        clipboardText: modelData === null ? "" : modelData.address
                        visible: modelData !== null
                        anchors.bottom: arrowLine.top
                        anchors.left: inIndex.right
                        anchors.leftMargin: 6
                        anchors.right: amount.left
                        anchors.rightMargin: 10
                    }
                    Flowee.BitcoinAmountLabel {
                        id: amount
                        visible: modelData !== null
                        value: modelData === null ? 0 : (-1 * modelData.value)
                        fiatTimestamp: root.minedDate
                        anchors.right: parent.right
                        anchors.bottom: arrowLine.top
                    }
                }
        }
    }
    Label {
        text: qsTr("Outputs") + ":"
        Layout.alignment: Qt.AlignTop
        visible: infoObject.outputs.length > 0
    }
    ColumnLayout {
        Layout.fillWidth: true
        Repeater {
            model: infoObject.outputs
            delegate:
                Item {
                    width: rightColumn.width
                    height: modelData === null ? 6 : (5 + outAddress.height)
                    Rectangle {
                        id: outArrowLine
                        /*
                         * There can be a nullptr, which means there is no info about this output.
                         * Then we have a bool 'forMe' which indicates the money goes to me or to
                         * someone else. Lets make the 'me' one the most visible one.
                         */
                        color: modelData === null ? "grey" : (modelData.forMe ? "#c5c537" : "#67671d")
                        anchors.left: parent.horizontalCenter
                        anchors.bottom: outArrowPoint.bottom
                        anchors.bottomMargin: 6
                        anchors.right: outArrowPoint.right
                        anchors.rightMargin: -1 // overlap the line and the arrow
                        height: 1.6
                    }
                    Flowee.ArrowPoint {
                        id: outArrowPoint
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        color: outArrowLine.color
                    }
                    Label {
                        id: outIndex
                        visible: modelData !== null
                        text: index
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.bottom: outArrowLine.top
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
                        anchors.left: outIndex.right
                        anchors.leftMargin: 6
                        anchors.rightMargin: 10
                        anchors.right: outAmount.left
                        anchors.bottom: outArrowLine.top
                    }
                    Flowee.BitcoinAmountLabel {
                        id: outAmount
                        visible: modelData !== null
                        value: modelData === null ? 0 : modelData.value
                        fiatTimestamp: root.minedDate
                        colorize: modelData !== null && modelData.forMe
                        anchors.right: outArrowPoint.left
                        anchors.rightMargin: 2
                        anchors.bottom: outArrowLine.top
                        anchors.bottomMargin: 6
                    }
                }
        }
    }
}
