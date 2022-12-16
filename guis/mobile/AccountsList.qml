/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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

Page {
    id: root
    headerText: qsTr("Wallet Information")

    ListView {
        id: tabBar
        model: {
            var accounts = portfolio.accounts;
            for (let a of portfolio.archivedAccounts) {
                accounts.push(a);
            }
            return accounts;
        }

        orientation: Qt.Horizontal
        width: parent.width
        anchors.top: parent.top
        height: 50
        boundsBehavior: Flickable.DragOverBounds

        delegate: Rectangle {
            color: {
                if (index == tabBar.currentIndex)
                    return "#1a1a1a"
                return "#303030"
            }
            width: Math.max(tabName.width, 120)
            height: tabName.height + 20

            Text {
                id: tabName
                color: index == tabBar.currentIndex ? "white" : "#c2c2c2"
                text: modelData.name
                anchors.centerIn: parent
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // fast move
                    accountPageListView.positionViewAtIndex(index, ListView.Beginning)
                    // slow scroll
                    tabBar.currentIndex = index;
                    accountPageListView.currentIndex = index;
                }
            }
        }
    }

    ListView {
        id: accountPageListView
        anchors.top: tabBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        orientation: Qt.Horizontal
        model: tabBar.model
        snapMode: ListView.SnapToItem
        boundsBehavior: Flickable.DragOverBounds
        clip: true
        cacheBuffer: 2
        onCurrentIndexChanged: tabBar.currentIndex = currentIndex
        onContentXChanged: {
            var pos = contentX;
            let index = pos / width;
            if (index !== Math.floor(index)) // its moving, ignore
                return;
            currentIndex = index;
        }

        delegate: Flickable {
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.DragOverBounds
            width: accountPageListView.width
            height: accountPageListView.height
            AccountPageListItem {
                width: accountPageListView.width
                account: modelData
            }
        }
    }
}
