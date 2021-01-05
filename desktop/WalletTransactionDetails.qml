/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

GridLayout {
    id: root

    property QtObject infoObject: null

    columns: 2
    Label {
        Layout.columnSpan: 2
        text: model.txid
        font.pointSize: 7
    }
    Label {
        text: qsTr("Status:")
    }
    Label {
        id: rightColumn
        Layout.fillWidth: true
        text: {
            if (typeof  model.date === "undefined")
                return qsTr("unconfirmed")
            return qsTr("%1 confirmations (mined in block %2)")
                .arg(Flowee.headerChainHeight - model.height).arg(model.height);
        }
    }

    Label {
        text: mainLabel.text
    }
    BitcoinAmountLabel {
        value: model.fundsOut - model.fundsIn
    }
    Label {
        text: qsTr("Size:")
    }
    Label {
        text: qsTr("%1 bytes").arg(infoObject.size)
    }
    Label {
        text: qsTr("Inputs:")
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
                    ArrowPoint {
                        id: arrowPoint
                        anchors.bottom: parent.bottom
                        color: arrowLine.color
                        width: 10
                        height: 20
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
                        color: "yellow"
                        visible: inAddress.visible
                        x: inAddress.x - 3
                        y: inAddress.y -3
                        height: inAddress.height + 6
                        width: Math.min(inAddress.width, inAddress.contentWidth) + 6
                        radius: height / 3
                        opacity: 0.2
                    }
                    Label {
                        id: inAddress
                        text: modelData === null ? "" : modelData.address
                        visible: modelData !== null
                        elide: Text.ElideMiddle
                        anchors.left: inIndex.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 20
                        anchors.right: amount.left
                        anchors.bottom: arrowLine.top
                    }
                    BitcoinAmountLabel {
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
        text: qsTr("Outputs:")
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
                        color: modelData === null ? "grey" : "#c5c537"
                        anchors.left: parent.horizontalCenter
                        anchors.bottom: outArrowPoint.bottom
                        anchors.bottomMargin: 9
                        anchors.right: outArrowPoint.right
                        anchors.rightMargin: -2 // overlap the line and the arrow
                        height: 3
                    }
                    ArrowPoint {
                        id: outArrowPoint
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        color: outArrowLine.color
                        width: 10
                        height: 20
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
                        color: "yellow"
                        visible: modelData !== null && modelData.forMe
                        x: outAddress.x - 3
                        y: outAddress.y -3
                        height: outAddress.height + 6
                        width: Math.min(outAddress.width, outAddress.contentWidth) + 6
                        radius: height / 3
                        opacity: 0.2
                    }
                    Label {
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
                    BitcoinAmountLabel {
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
