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
import Flowee.org.pay 1.0

ApplicationWindow {
    id: root
    visible: true
    minimumWidth: 300
    minimumHeight: 200
    width: 400
    height: grid.height + 30 + header.height
    title: qsTr("Create New Account")
    modality: Qt.NonModal
    flags: Qt.Dialog

    function validate() {
        if (privKeyButton.checked) {
            var typedData = Flowee.identifyString(privKey.text)
            feedback.ok = typedData === Pay.PrivateKey;
        } else {
            feedback.ok = true;
        }
    }

    Label {
        id: header
        text: qsTr("Please check which kind of account you want to create:")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 10
    }

    GridLayout {
        id: grid
        columns: 3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 10
        anchors.top: header.bottom

        Keys.onPressed: {
            if (event.key === Qt.Key_Escape) {
                root.visible = false;
                event.accepted = true
            }
        }

        Label { text: qsTr("Name:") }
        TextField {
            id: accountName
            focus: true
            Layout.fillWidth: true
            Layout.columnSpan: 2
            onAccepted: if (okButton.enabled) okButton.clicked()
        }
        RadioButton {
            id: emptyAccountButton
            Layout.columnSpan: 3
            text: qsTr("New, Empty Account")
            onCheckedChanged: root.validate()
            checked: true
        }
        RadioButton {
            id: privKeyButton
            Layout.columnSpan: 3
            text: qsTr("Imported Private Key")
            onCheckedChanged: root.validate()
        }
        Pane { Layout.fillWidth: false }
        TextField {
            id: privKey
            enabled: privKeyButton.checked
            placeholderText: "L5bxhjPeQqVFgCLALiFaJYpptdX6Nf6R9TuKgHaAikcNwg32Q4aL"
            Layout.fillWidth: true
            onTextChanged: root.validate()
        }
        Label {
            id: feedback
            property bool ok: false
            text: ok ?  "✔" : " "
            color: "green"
            font.pixelSize: 24
        }
        Pane {
            Layout.columnSpan: 3
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom

            RowLayout {
                Button2 {
                    id: okButton
                    text: qsTr("&Ok")
                    enabled: accountName.text.length > 2 && (emptyAccountButton.checked || feedback.ok)
                    onClicked: {
                        if (privKeyButton.checked)
                            Flowee.createImportedWallet(privKey.text, accountName.text)
                        else
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
                Button2 {
                    text: qsTr("&Cancel")
                    onClicked: root.visible = false
                }
            }
        }
    }
}
