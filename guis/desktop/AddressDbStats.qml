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
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee

QQC2.ApplicationWindow {
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

        QQC2.Label {
            text: qsTr("Total found") + ":"
            Layout.alignment: Qt.AlignRight
        }
        QQC2.Label {
            text: root.stats.count;
            Layout.fillWidth: true 
        }
        QQC2.Label {
            text: qsTr("Tried") + ":"
            Layout.alignment: Qt.AlignRight
        }
        QQC2.Label {
            text: root.stats.everConnected;
        }
        QQC2.Label {
            text: qsTr("Punished count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        QQC2.Label {
            text: root.stats.partialBanned;
        }
        QQC2.Label {
            text: qsTr("Banned count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        QQC2.Label {
            text: root.stats.banned;
        }

        QQC2.Label {
            text: qsTr("IP-v4 count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        QQC2.Label {
            text: root.stats.count - root.stats.ipv6Addresses
            font.strikeout: root.stats.usesIPv4 === false
        }
        QQC2.Label {
            text: qsTr("IP-v6 count") + ":"
            Layout.alignment: Qt.AlignRight
        }
        QQC2.Label {
            text: root.stats.ipv6Addresses
            font.strikeout: root.stats.usesIPv6 === false
        }

        Flowee.Button {
            Layout.columnSpan: 2
            text: qsTr("Pardon the Banned")
            onClicked: {
                net.pardonBanned();
                var newStats = net.createStats(root);
                root.stats.count = newStats.count;
                root.stats.everConnected = newStats.everConnected;
                root.stats.partialBanned = newStats.partialBanned;
                root.stats.banned = newStats.banned;
                root.stats.ipv6Addresses = newStats.ipv6Addresses;
                root.stats.usesIPv6 = newStats.usesIPv6;
                root.stats.usesIPv4 = newStats.usesIPv4;
                enabled = false;
            }
        }
    }

    footer: Item {
        width: parent.width
        height: closeButton.height + 20

        Flowee.Button {
            id: closeButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: qsTr("Close")
            onClicked: root.visible = false
        }
    }
}
