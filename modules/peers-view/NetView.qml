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
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee
import "../mobile";

Page {
    headerText: qsTr("Peers")
    ListView {
        id: listView
        model: net.peers
        anchors.fill: parent
        QQC2.ScrollBar.vertical: QQC2.ScrollBar { }

        focus: true
        Keys.onPressed: (event)=> {
            if (event.key === Qt.Key_Escape) {
                thePile.pop();
                event.accepted = true
            }
        }

        delegate: Rectangle {
            width: listView.width
            height: peerPane.height + 12
            color: index % 2 === 0 ? palette.button : palette.base
            opacity: modelData.headersReceived ? 1 : 0.5
            ColumnLayout {
                id: peerPane
                width: listView.width - 20
                x: 10
                y: 6

                Flowee.Label {
                    text: modelData.userAgent
                }
                Flowee.Label {
                    text: qsTr("Address", "network address (IP)") + ": " + modelData.address
                }
                RowLayout {
                    height: secondRow.height
                    Flowee.Label {
                        id: secondRow
                        text: qsTr("Start-height: %1").arg(modelData.startHeight)
                    }
                    Flowee.Label {
                        text: qsTr("ban-score: %1").arg(modelData.banScore)
                    }
                }
                Flowee.Label {
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
                            return ""
                        }
                        var accounts = portfolio.accounts;
                        for (var i = 0; i < accounts.length; ++i) {
                            if (accounts[i].id === id)
                                return qsTr("Peer for wallet: %1").arg(accounts[i].name);
                        }
                        return qsTr("Peer for wallet");
                    }
                    visible: text !== ""
                }
                Flow {
                    Repeater {
                        model: modelData.services
                        delegate: Flowee.Label { text: modelData }
                    }
                }
            }
        }
        Keys.forwardTo: Flowee.ListViewKeyHandler {
            target: listView
        }
    }
}