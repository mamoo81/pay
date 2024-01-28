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
import "../mobile" as Mobile;
import Flowee.org.pay;

Mobile.Page {
    id: root
    headerText: qsTr("Peers") + " (" + listView.count +")";

    property QtObject statsAction : QQC2.Action {
        text: qsTr("Statistics")
        onTriggered: thePile.push("./StatsPage.qml", {"stats": net.createStats(root) });
    }

    menuItems: [statsAction];


    ListView {
        id: listView
        model: net
        anchors.fill: parent
        QQC2.ScrollBar.vertical: QQC2.ScrollBar { }

        focus: true
        Keys.onPressed: (event)=> {
            if (event.key === Qt.Key_Escape) {
                thePile.pop();
                event.accepted = true
            }
        }
        // make removals animated to avoid the list looking 'shokking'
        removeDisplaced: Transition {
            NumberAnimation { properties: "y"; duration: 300; easing.type: Easing.InQuad }
        }

        delegate: Rectangle {
            width: listView.width
            height: peerPane.height + 12
            color: index % 2 === 0 ? palette.button : palette.base
            border.width: net.selectedId === model.connectionId ? 2 : 0
            border.color: palette.highlight
            opacity: {
                let validity = model.validity;
                if (validity === Wallet.CheckedOk || validity === Wallet.KnownGood)
                    return 1;
                return 0.7;
            }
            ColumnLayout {
                id: peerPane
                width: listView.width - 20
                x: 10
                y: 6

                Flowee.Label {
                    text: {
                        var t = model.userAgent;
                        if (t === "")
                            t = model.connectionId
                        return t;
                    }
                }
                Flowee.Label {
                    text: qsTr("Address", "network address (IP)") + ": " + model.address
                }
                RowLayout {
                    height: secondRow.height
                    spacing: 0
                    Flowee.Label {
                        id: secondRow
                        text: qsTr("Start-height: %1").arg(model.startHeight)
                    }
                    Flowee.Label {
                        text: ", " + qsTr("ban-score: %1").arg(model.banScore)
                    }
                }
                Flowee.Label {
                    id : accountStatus
                    font.bold: true
                    text: {
                        var id = model.segment;
                        if (id === 0) {
                            let validity = model.validity;
                            if (validity === Wallet.OpeningConnection)
                                return qsTr("Opening Connection");
                            if (validity === Wallet.Checking)
                                return qsTr("Validating peer");
                            if (validity === Wallet.CheckedOk)
                                return qsTr("Validated");
                            if (validity === Wallet.KnownGood)
                                return qsTr("Good Peer");
                            return ""; // unknown
                        }
                        var accounts = portfolio.rawAccounts;
                        for (var i = 0; i < accounts.length; ++i) {
                            if (accounts[i].id === id)
                                return qsTr("Peer for wallet: %1").arg(accounts[i].name);
                        }
                        return "Internal Error";
                    }
                }
                Flowee.Label {
                    text: "Downloading!"
                    visible: model.isDownloading
                }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (event)=> {
                    // for mouse
                    net.selectedId = model.connectionId;
                    if (event.button === Qt.RightButton)
                        peerContextMenu.popup(parent, event.x, event.y)
                }
                onPressAndHold: (event) => {
                    // for touch
                    net.selectedId = model.connectionId;
                    peerContextMenu.popup(parent, event.x, event.y)
                }
            }
            QQC2.Menu {
                id: peerContextMenu
                QQC2.MenuItem {
                    text: qsTr("Disconnect Peer")
                    onClicked: net.disconnectPeer(model.connectionId);
                }
                QQC2.MenuItem {
                    text: qsTr("Ban Peer")
                    onClicked: net.banPeer(model.connectionId);
                }
            }
        }
        Keys.forwardTo: Flowee.ListViewKeyHandler {
            target: listView
        }
    }
}
