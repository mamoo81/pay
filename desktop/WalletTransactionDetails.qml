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
import QtQuick.Layouts 1.11
import "widgets" as Flowee

GridLayout {
    id: root

    property QtObject infoObject: null

    columns: 2
    LabelWithClipboard {
        Layout.columnSpan: 2
        text: model.txid
        font.pointSize: 7
    }
    Label {
        text: qsTr("Status") + ":"
    }
    LabelWithClipboard {
        id: rightColumn
        Layout.fillWidth: true
        text: {
            if (txRoot.isRejected)
                return qsTr("rejected")
            if (typeof  model.date === "undefined")
                return qsTr("unconfirmed")
            var confirmations = Pay.headerChainHeight - model.height + 1;
            return qsTr("%1 confirmations (mined in block %2)", "", confirmations)
                .arg(confirmations).arg(model.height);
        }
        clipboardText: model.height
    }

    Label {
        text: mainLabel.text + ":"
    }
    Flowee.BitcoinAmountLabel {
        value: model.fundsOut - model.fundsIn
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
    }
    ColumnLayout {
        Layout.fillWidth: true
        Repeater {
            model: infoObject.inputs
            delegate:
                Item {
                    width: rightColumn.width
                    height: modelData === null ? 8 : (10 + inAddress.height)
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
                        anchors.bottomMargin: 9
                        anchors.right: parent.horizontalCenter
                        height: 3
                    }
                    Label {
                        id: inIndex
                        text: index
                        visible: modelData !== null
                        anchors.left: arrowPoint.right
                        anchors.leftMargin: 10
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
                    LabelWithClipboard {
                        id: inAddress
                        text: modelData === null ? "" : modelData.address
                        visible: modelData !== null
                        anchors.bottom: arrowLine.top
                        anchors.left: inIndex.right
                        anchors.leftMargin: 10
                        anchors.right: amount.left
                        anchors.rightMargin: 20
                    }
                    Flowee.BitcoinAmountLabel {
                        id: amount
                        visible: modelData !== null
                        value: modelData === null ? 0 : (-1 * modelData.value)
                        fontPtSize: date.font.pointSize
                        anchors.right: parent.right
                        anchors.bottom: arrowLine.top
                    }
                }
        }
    }
    Label {
        text: qsTr("Outputs") + ":"
        Layout.alignment: Qt.AlignTop
    }
    ColumnLayout {
        Layout.fillWidth: true
        Repeater {
            model: infoObject.outputs
            delegate:
                Item {
                    width: rightColumn.width
                    height: modelData === null ? 12 : (10 + outAddress.height)
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
                        anchors.bottomMargin: 9
                        anchors.right: outArrowPoint.right
                        anchors.rightMargin: -2 // overlap the line and the arrow
                        height: 3
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
                        anchors.leftMargin: 10
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
                    LabelWithClipboard {
                        id: outAddress
                        visible: modelData !== null
                        elide: Text.ElideMiddle
                        text: modelData === null ? "" : modelData.address
                        anchors.left: outIndex.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 20
                        anchors.right: outAmount.left
                        anchors.bottom: outArrowLine.top
                    }
                    Flowee.BitcoinAmountLabel {
                        id: outAmount
                        visible: modelData !== null
                        value: modelData === null ? 0 : modelData.value
                        colorize: modelData !== null && modelData.forMe
                        fontPtSize: date.font.pointSize
                        anchors.right: parent.right
                        anchors.bottom: outArrowLine.top
                        anchors.bottomMargin: 10
                    }
                }
        }
    }
}
