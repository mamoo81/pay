/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
import Flowee.org.pay 1.0

Item {
    Label {
        id: title
        text: qsTr("This creates a new empty wallet with simple multi-address capability")
        width: parent.width
    }
    RowLayout {
        id: nameRow
        anchors.top: title.bottom
        anchors.topMargin: 10
        Label {
            text: qsTr("Name:");
            Layout.alignment: Qt.AlignBaseline
        }
        FloweeTextField {
            id: accountName
        }
    }
    Button2 {
        id: button
        text: qsTr("Go")
        anchors.top: nameRow.bottom
        anchors.right: parent.right
        enabled: accountName.text.length > 2
        onClicked: {
            Flowee.createNewWallet(accountName.text);

            var accounts = portfolio.accounts;
            for (var i = 0; i < accounts.length; ++i) {
                var a = accounts[i];
                if (a.name === accountName.text) {
                    portfolio.current = a;
                    break;
                }
            }
            root.visible = false;
        }
    }

    FloweeGroupBox {
        title: qsTr("Advanced Options")
        anchors.top: button.bottom
        anchors.topMargin: 10
        width: parent.width
        isCollapsed: true
        columns: 1

        FloweeCheckBox {
            id: singleAddress
            text: qsTr("Force Single Address");
            tooltipText: qsTr("When enabled, this wallet will be limited to one address.\nThis ensures only one private key will need to be backed up")
        }
        FloweeCheckBox {
            id: schnorr
            text: qsTr("Default to signing using ECDSA");
            tooltipText: qsTr("When enabled, newer style Schnorr signatures are not set as default for this wallet.")
        }
    }
}
