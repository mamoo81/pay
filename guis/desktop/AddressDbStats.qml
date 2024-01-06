/*
 * This file is part of the Flowee project
 * Copyright (C) 2024 Tom Zander <tom@flowee.org>
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
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee

ApplicationWindow {
    id: root
    visible: false
    minimumWidth: 200
    minimumHeight: 200
    width: 400
    height: 300
    title: qsTr("IP Addresses");
    modality: Qt.NonModal
    flags: Qt.Dialog

    property QtObject stats: net.createStats(root);

    GridLayout {
        columns: 2
        rowSpacing: 10
        columnSpacing: 6
        width: parent.width - 20
        x: 10
        y: 10

        Label {
            text: qsTr("Total found") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: root.stats.count;
            Layout.fillWidth: true 
        }
        Label {
            text: qsTr("Tried") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: root.stats.everConnected;
        }
        Label {
            text: qsTr("Punished count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: root.stats.partialBanned;
        }
        Label {
            text: qsTr("Banned count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: root.stats.banned;
        }

        Label {
            text: qsTr("IP-v4 count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: root.stats.count - root.stats.ipv6Addresses
            font.strikeout: root.stats.usesIPv4 === false
        }
        Label {
            text: qsTr("IP-v6 count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        Label {
            text: root.stats.ipv6Addresses
            font.strikeout: root.stats.usesIPv6 === false
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
