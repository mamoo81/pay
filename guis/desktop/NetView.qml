/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee

ApplicationWindow {
    id: root
    visible: false
    minimumWidth: 200
    minimumHeight: 200
    width: 400
    height: 500
    title: qsTr("Peers (%1)", "", net.peers.length).arg(net.peers.length)
    modality: Qt.NonModal
    flags: Qt.Dialog


    ListView {
        id: listView
        model: net.peers
        clip: true
        ScrollBar.vertical: ScrollBar { }

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
            color: index % 2 === 0 ? secondRow.palette.button : secondRow.palette.base
            ColumnLayout {
                id: peerPane
                width: listView.width - 20
                x: 10
                y: 6

                Label {
                    text: modelData.userAgent
                }
                Label {
                    text: qsTr("Address", "network address (IP)") + ": " + modelData.address
                }
                RowLayout {
                    height: secondRow.height
                    Label {
                        id: secondRow
                        text: qsTr("Start-height: %1").arg(modelData.startHeight)
                    }
                    Label {
                        text: qsTr("ban-score: %1").arg(modelData.banScore)
                    }
                }
                Label {
                    id : accountStatus
                    font.bold: true
                    text: {
                        var id = modelData.segmentId;
                        if (id === 0) {
                            // not peered yet.
                            if (modelData.services.Length === 0)
                                return qsTr("initializing connection")
                            if (!modelData.headersReceived)
                                return qsTr("Verifying peer")
                            return qsTr("Assigning...")
                        }
                        var accounts = portfolio.accounts;
                        for (var i = 0; i < accounts.length; ++i) {
                            if (accounts[i].id === id)
                                return qsTr("Peer for wallet: %1").arg(accounts[i].name);
                        }
                        return qsTr("Peer for initial wallet");
                    }
                    visible: text !== ""
                }
                Flow {
                    Repeater {
                        model: modelData.services
                        delegate: Label { text: modelData }
                    }
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

        Button {
            id: closeButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: qsTr("Close")
            onClicked: root.visible = false
        }
    }
}
