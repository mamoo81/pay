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

Item {
    id: netView

    Rectangle { // TODO replace with nice button. Opens the 'net' pane
        color: "pink";
        width: 40
        height: 40
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        MouseArea {
            anchors.fill: parent
            onClicked: peerList.x = netView.width - peerList.width
        }
    }

    Item {
        visible: opacity === 1
        opacity: peerList.x == width ? 0 : 1
        anchors.fill: parent

        MouseArea {
            // to close the 'net' screen
            anchors.fill: parent
            onClicked: peerList.x = netView.width
        }


        Rectangle {
            id: peerList
            width: parent.width / 3
            height: parent.height
            x: parent.width
            color: "#aaffffff"
            border.width: 3
            border.color: "black";

            Text {
                id: netTitle
                text: "Peers (" + net.peers.length + ")"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            ListView {
                model: net.peers
                anchors.top: netTitle.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right

                delegate: Item {
                    id: peerPane
                    width: peerList.width
                    height: mainLabel.height + secondRow.height + 20

                    Text {
                        id: mainLabel
                        font.pointSize: 12
                        text: modelData.userAgent
                    }
                    Row {
                        anchors.topMargin: 10
                        anchors.top: mainLabel.bottom
                        spacing: 10
                        Text {
                            id: secondRow
                            font.pointSize: 10
                            color: "#444444"
                            text: qsTr("Start-height: %1").arg(modelData.startHeight)
                        }
                        Text {
                            font.pointSize: 10
                            color: "#444444"
                            text: qsTr("ban-score: %1").arg(modelData.banScore)
                        }
                    }
                }
            }
        }
    }
}
