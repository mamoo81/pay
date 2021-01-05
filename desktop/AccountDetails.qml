/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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

ApplicationWindow {
    id: root
    visible: true
    minimumWidth: 300
    minimumHeight: 400
    width: 400
    height: grid.height + 20
    title: qsTr("Account Details")
    modality: Qt.NonModal
    flags: Qt.Dialog
    
    property QtObject account: null

    GridLayout {
        id: grid
        columns: 2
        anchors.fill: parent
        anchors.margins: 10

        Keys.onPressed: {
            if (event.key === Qt.Key_Escape) {
                root.visible = false;
                event.accepted = true
            }
        }
        Label {
            id: header
            text: {
                // ok we only have one type so far, so this is rather simple...
                qsTr("This account is a single-address wallet.")
            }
            Layout.columnSpan: 2
        }
        
        Label { text: qsTr("Name:") }
        TextField {
            text: root.account.name
            focus: true
            Layout.fillWidth: true
            onAccepted:  root.account.name = text
        }
        Label {
            text: qsTr("Balance unconfirmed:")
        }
        BitcoinAmountLabel {
            value: root.account.balanceUnconfirmed
            colorize: false
        }
        Label {
            text: qsTr("Balance immature:")
        }
        BitcoinAmountLabel {
            value: root.account.balanceImmature
            colorize: false
        }
        Label {
            text: qsTr("Balance other:")
        }
        BitcoinAmountLabel {
            value: root.account.balanceConfirmed
            colorize: false
        }
        Label {
            text: qsTr("Unspent coins:")
        }
        Label {
            text: root.account.unspentOutputCount
        }
        Label {
            text: qsTr("Historical coins:")
        }
        Label {
            text: root.account.historicalOutputCount
        }
        Label {
            text: qsTr("Sync status:")
        }
        SyncIndicator {
            id: syncIndicator
            accountBlockHeight: root.account === null ? 0 : root.account.lastBlockSynched
        }
        Pane {}
        Label {
            text: syncIndicator.accountBlockHeight + " / " + syncIndicator.globalPos
        }

        Item {
            Layout.columnSpan: 2
            width: closeButton.width
            height: closeButton.height
            Layout.fillHeight: true
            Layout.fillWidth: true

            Button {
                id: closeButton
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Close")
                onClicked: root.visible = false
            }
        }
    }
}
