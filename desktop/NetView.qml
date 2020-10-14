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
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import QtQuick.Window 2.15

ApplicationWindow {
    id: netView
    visible: false
    minimumWidth: 300
    minimumHeight: 400
    width: 500
    height: 400
    title: qsTr("Peers (%1)").arg(net.peers.length)
    modality: Qt.NonModal
    flags: Qt.Dialog

    ListView {
        id: peerList
        model: net.peers
        anchors.fill: parent
        anchors.margins: 10

        delegate: Rectangle {
            width: peerList.width
            height: peerPane.height + 12
            color: index % 2 === 0 ? netView.palette.button : netView.palette.alternateBase
            ColumnLayout {
                id: peerPane
                width: peerList.width
                y: 6

                Label {
                    id: mainLabel
                    text: modelData.userAgent
                }
                RowLayout {
                    id: rowLayout
                    height: secondRow.height
                    Label {
                        id: secondRow
                        text: qsTr("Start-height: %1").arg(modelData.startHeight)
                    }
                    Label {
                        text: qsTr("ban-score: %1").arg(modelData.banScore)
                    }
                }
            }
        }
    }
}
