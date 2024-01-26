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
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import "../mobile" as Mobile

Mobile.Page {
    id: root
    headerText: qsTr("IP-Address Statistics");

    required property QtObject stats;

    Column {
        width: parent.width
        spacing: 10

        Mobile.PageTitledBox {
            title: qsTr("Counts")
            width: parent.width
            Flowee.Label {
                text: qsTr("Total found") + ": " + root.stats.count;
            }
            Flowee.Label {
                text: qsTr("Tried") + ": " + root.stats.everConnected;
            }
        }
        Mobile.PageTitledBox {
            title: qsTr("Misbehaving IPs")
            width: parent.width
            Flowee.Label {
                text: qsTr("Bad") + ": " + root.stats.partialBanned;
            }
            Flowee.Label {
                text: qsTr("Banned") + ": " + root.stats.banned;
            }
        }

        Mobile.PageTitledBox {
            title: qsTr("Network")
            width: parent.width
            Flowee.Label {
                text: "IP-v4" + ": " + (root.stats.count - root.stats.ipv6Addresses)
                font.strikeout: root.stats.usesIPv4 === false
            }
            Flowee.Label {
                text: "IP-v6" + ": " + root.stats.ipv6Addresses
                font.strikeout: root.stats.usesIPv6 === false
            }
        }

        Flowee.Button {
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
}
