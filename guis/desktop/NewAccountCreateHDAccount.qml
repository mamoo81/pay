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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Flowee" as Flowee

ColumnLayout {
    id: newAccountCreateHDAccount
    spacing: 10
    Label {
        id: title
        text: qsTr("This creates a new empty wallet with smart creation of addresses from a single seed-phrase")
        width: parent.width
    }
    RowLayout {
        id: nameRow
        Label {
            text: qsTr("Name") + ":";
            Layout.alignment: Qt.AlignBaseline
        }
        Flowee.TextField {
            id: accountName
        }
    }
    Item {
        height: button.height
        Layout.fillWidth: true

        Flowee.Button {
            id: button
            text: qsTr("Go")
            anchors.right: parent.right
            onClicked: {
                var options = Pay.createNewWallet(derivationPath.text, /* password */"", accountName.text);
                for (let a of portfolio.accounts) {
                    if (a.id === options.accountId) {
                        portfolio.current = a;
                        break;
                    }
                }
                root.visible = false;
            }
        }
    }

    Flowee.GroupBox {
        title: qsTr("Advanced Options")
        Layout.fillWidth: true
        collapsed: true
        columns: 3

        /*
        Flowee.CheckBox {
            id: schnorr
            text: qsTr("Default to signing using ECDSA");
            toolTipText: qsTr("When enabled, newer style Schnorr signatures are not set as default for this wallet.")
            Layout.columnSpan: 2
        } */
        Label {
            text: qsTr("Derivation") + ":"
            Layout.fillWidth: false
        }
        Flowee.TextField {
            id: derivationPath
            text: "m/44'/0'/0'" // What most wallets use to import by default
            color: Pay.checkDerivation(text) ? palette.text : "red"
        }
        Item { width: 1; height: 1; Layout.fillWidth: true } // spacer
    }
}
