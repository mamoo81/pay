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
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee

Page {
    id: root
    headerText: portfolio.singleAccountSetup ? qsTr("Wallet") : qsTr("Wallets")

    property QtObject newAccountAction: QQC2.Action {
        text: qsTr("Add Wallet")
        onTriggered: thePile.push("./NewAccount.qml")
    }
    menuItems: [ newAccountAction ]

    function indexOfCurrentAccount() {
        var list = tabBar.model;
        var cur = portfolio.current
        for (let i = 0; i < list.length; i = i + 1) {
            if (list[i] === cur) return i;
        }
        return 0;
    }

    Column {
        id: topGrid
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !portfolio.singleAccountSetup
        height: visible ? implicitHeight: 0

        Flowee.CheckBox {
            text: qsTr("Private Mode")
            toolTipText: qsTr("You can hide private wallets when you hand over your phone")
            checked: Pay.privateMode
            onClicked: Pay.privateMode = checked
        }
        TextButton {
            showPageIcon: true
            text: qsTr("Default Wallet")
            subtext: {
                for (let a of portfolio.accounts) {
                    if (a.isPrimaryAccount) {
                        var defaultAccount = a.name;
                        break;
                    }
                }

                qsTr("%1 is used on startup").arg(defaultAccount);
            }
            onClicked: ; // TODO
        }
    }

    ListView {
        id: tabBar
        model: {
            var accounts = portfolio.accounts;
            for (let a of portfolio.archivedAccounts) {
                accounts.push(a);
            }
            if (accounts.length === 0)
                accounts.push(portfolio.current)
            return accounts;
        }

        orientation: Qt.Horizontal
        width: parent.width
        anchors.top: topGrid.bottom
        anchors.topMargin: 10
        height: portfolio.accounts.length > 1 ? 50 : 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: indexOfCurrentAccount();

        delegate: Item {
            width: Math.max(tabName.width, 120)
            height: tabName.height + 20

            Rectangle {
                x: 5
                height: 4
                width: parent.width - 10
                color: palette.highlight
                visible: index === tabBar.currentIndex
                anchors.bottom: parent.bottom
            }
            Rectangle {
                anchors.fill: parent
                color: palette.highlight
                visible: index === tabBar.currentIndex
                opacity: 0.15
            }

            Text {
                id: tabName
                color: index === tabBar.currentIndex ? palette.windowText : palette.brightText;
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
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        cacheBuffer: 2
        currentIndex: indexOfCurrentAccount();
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
            contentHeight: item.height
            AccountPageListItem {
                id: item
                width: accountPageListView.width
                account: modelData
                clip: true
            }
        }
    }
}
