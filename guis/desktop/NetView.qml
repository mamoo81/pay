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
import QtQuick.Controls as QQC2;
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;

QQC2.ApplicationWindow {
    id: root
    visible: false
    minimumWidth: 200
    minimumHeight: 200
    width: 400
    height: 500
    title: qsTr("Peers (%1)", "", listView.count).arg(listView.count)
    modality: Qt.NonModal
    flags: Qt.Dialog


    ListView {
        id: listView
        model: net
        clip: true
        QQC2.ScrollBar.vertical: QQC2.ScrollBar { }

        anchors.fill: parent
        focus: true
        Keys.onPressed: (event)=> {
            if (event.key === Qt.Key_Escape) {
                root.visible = false;
                event.accepted = true
            }
        }

        delegate: Rectangle {
            width: listView.width
            height: peerPane.height + 12
            border.width: net.selectedId === model.connectionId ? 2 : 0
            border.color: palette.highlight
            color: index % 2 === 0 ? palette.button : palette.base
            opacity: {
                let validity = model.validity;
                if (validity === Wallet.Checking)
                    return 0.7;
                return 1;
            }
            QQC2.Label {
                text: "(" + model.connectionId + ")"
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 20
            }

            ColumnLayout {
                id: peerPane
                width: listView.width - 40
                x: 10
                y: 6

                QQC2.Label {
                    text: model.userAgent
                }
                QQC2.Label {
                    text: qsTr("Address", "network address (IP)") + ": " + model.address
                }
                RowLayout {
                    height: secondRow.height
                    spacing: 0
                    QQC2.Label {
                        id: secondRow
                        text: qsTr("Start-height: %1").arg(model.startHeight)
                    }
                    QQC2.Label {
                        text: ", " + qsTr("ban-score: %1").arg(model.banScore)
                    }
                }
                QQC2.Label {
                    id : accountStatus
                    font.bold: true
                    text: {
                        var id = model.segment;
                        if (id === 0) {
                            let validity = model.validity;
                            if (validity === Wallet.OpeningConnection)
                                return "Opening Connection";
                            if (validity === Wallet.Checking)
                                return "Validating peer";
                            if (validity === Wallet.CheckedOk)
                                return "Validated";
                            if (validity === Wallet.KnownGood)
                                return "Good Peer";
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
                QQC2.Label {
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

    footer: Item {
        width: parent.width
        height: closeButton.height + 20

        QQC2.Button {
            id: closeButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: qsTr("Close")
            onClicked: root.visible = false
        }
    }
}
