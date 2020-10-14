/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

/*
  This shows all the wallets in a list to select one from.
  Each wallet shows its name, historical data, its current value etc.
 */
ListView {
    id: root
    model: walletsView.active ? wallets.accounts : null
    width: childWidth * Math.min(count, 3)
    snapMode: ListView.SnapOneItem
    orientation: Qt.Horizontal
    focus: true
    currentIndex: count > 2 ? 1 : 0

    property int childWidth: 200

    delegate: Item {
        width: root.childWidth
        height: root.height

        property bool current: ListView.isCurrentItem

        Rectangle {
            color: "#eafeed"
            border.width: 3
            radius: 20
            border.color: "#163518"
            clip: true
            y: (parent.height - height) / 2
            height: parent.current ? root.height : root.height * 0.6
            width: parent.width - 20
            x: 10

            Column {
                id: top
                x: 10
                y: 10
                width: parent.width - 20
                spacing: 7
                Text {
                    id: walletTitle
                    text: modelData ? modelData.name : ""
                    font.pixelSize: 28
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
                Row {
                    height: balanceLabel.height
                    spacing: 6
                    Text {
                        id: balanceLabel
                        text: qsTr("Balance:")
                    }
                    BitcoinAmountLabel {
                        value: modelData.balance
                        colorize: false
                    }
                }
            }
            ListView {
                width: parent.width - 20
                x: 10
                anchors.top: top.bottom
                anchors.topMargin: 20
                anchors.bottom: parent.bottom
                model: modelData.transactions
                visible: top.visible

                delegate: WalletTransaction {
                    id: walletTransaction
                    width: parent.width
                }
            }
            Behavior on height { NumberAnimation { duration: 120 } }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.__lastSelected = root.currentIndex
                    wallets.current = modelData
                }
            }
        }
    }

    property int __lastSelected: -1 // remember position of list
    onCountChanged: {
        var index = currentIndex
        if (index === 0 && count > 1) {
            index = __lastSelected
            if (index < 0)
                index = 1
            currentIndex = index;
        }
        positionViewAtIndex(index, ListView.Center)
    }
    onCurrentIndexChanged: {
        // ensure that the current index stays visible and centered
        positionViewAtIndex(currentIndex, ListView.Center)
    }
    onMovementEnded: {
        var centerOfView = mapToGlobal(width / 2, height / 2)
        for (var i = 0; i < count; ++i ) {
            var item = itemAtIndex(i)
            if (item) {
                var adjustedPos = item.mapFromGlobal(centerOfView.x, centerOfView.y)
                if (item.contains(adjustedPos)) {
                    currentIndex = i
                    return;
                }
            }
        }
    }
}

