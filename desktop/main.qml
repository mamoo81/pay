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
import QtQml.StateMachine 1.15 as DSM
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import QtQuick.Dialogs 1.3
import Flowee.org.pay 1.0

// import QtQuick.VirtualKeyboard 2.14

ApplicationWindow {
    id: window
    visible: true
    width: Flowee.windowWidth
    height: Flowee.windowHeight
    title: "Flowee Pay"
    // color: darkTheme.checked ? "black" : "white"

    onWidthChanged: Flowee.windowWidth = width
    onHeightChanged: Flowee.windowHeight = height


    DSM.StateMachine {
        id: globalStateMachine
        initialState: splash
        running: true

        DSM.State {
            id: splash
            property bool loaded: wallets !== null

            DSM.SignalTransition {
                targetState: walletsView
                signal: splash.onLoadedChanged
                guard: splash.loaded
            }
        }

        DSM.State {
            id: walletsView
            property bool hasWallet: {
                if (typeof wallets === 'undefined') {
                    return false
                }
                return wallets.current !== null
            }

            DSM.SignalTransition {
                targetState: walletDetails
                signal: walletsView.onHasWalletChanged
            }

            DSM.SignalTransition {
                targetState: walletDetails
                signal: wallets.onCurrentChanged
                guard: wallets !== null && wallets.current !== null
            }
        }

        DSM.State {
            id: walletDetails
            DSM.SignalTransition {
                targetState: walletsView
                signal: wallets.onCurrentChanged
                guard: wallets !== null && wallets.current === null
            }
        }
    }

    Rectangle {
        id: back
        width: 50
        height: width
        color: "lightgrey"
        visible: opacity !== 0
        opacity: walletDetails.active ? 1 : 0
        Text {
            text: "Back"
            anchors.centerIn: parent
        }
        MouseArea {
            visible: wallets.current !== null
            anchors.fill: parent
            onClicked:  wallets.current = null
        }

        Behavior on opacity { NumberAnimation { duration: 350 } }
    }

    Item {
        id: loadingIndicator
        anchors.centerIn: parent
        visible: splash.active

        Image {
            source: "qrc:///FloweePay.png"
            width: 250
            height: 250
            fillMode: Image.PreserveAspectFit
            anchors.centerIn: parent
        }
    }

    Item {
        id: mainContent
        visible: !splash.active
        width: parent.width
        height: parent.height

        Text {
            id: title
            anchors.margins: 5
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Flowee Pay"
            font.pointSize: 24
        }

        WalletsOverview {
            id: walletsOverview
            visible: opacity !== 0
            opacity: walletsView.active ? 1 : 0

            anchors.bottom: parent.bottom
            anchors.top: buttonBar.bottom
            anchors.margins: 10
            anchors.horizontalCenter: parent.horizontalCenter
            childWidth:  window.width / 3
        }
        Rectangle {
            id: addWalletButton
            anchors.bottom: walletsOverview.bottom
            x: 50
            visible: walletsOverview.visible
            opacity: walletsOverview.opacity
            width: 70
            height: width
            border.color: "#c9fbff"
            border.width: 1
            color: "white"
            Rectangle {
                color: "black"
                width: 2
                height: parent.height * 0.8
                anchors.centerIn: parent
            }
            Rectangle {
                color: "black"
                height: 2
                width: parent.height * 0.8
                anchors.centerIn: parent
            }

            MouseArea {
                anchors.fill: parent
                onClicked: createNewWalletDialog.showDialog()
            }

            Behavior on opacity { NumberAnimation { duration: 350 } }
        }

        Rectangle {
            // prev button
            width: 40
            height: 40
            anchors.top: walletsOverview.top
            anchors.topMargin: 30
            color: "#77FFFFFF"
            visible: walletsOverview.visible
            opacity: walletsOverview.opacity
            enabled: walletsOverview.currentIndex > 1
            Text {
                text: "<"
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                color: parent.enabled ? "black" : "#aaaaaa"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: walletsOverview.decrementCurrentIndex()
            }
        }

        Rectangle { // next button
            width: 40
            height: 40
            anchors.right: parent.right
            anchors.top: walletsOverview.top
            anchors.topMargin: 30

            color: "#77FFFFFF"
            visible: walletsOverview.visible
            opacity: walletsOverview.opacity
            enabled: walletsOverview.currentIndex < walletsOverview.count - 2
            Text {
                text: ">"
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 10
                color: parent.enabled ? "black" : "#aaaaaa"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: walletsOverview.incrementCurrentIndex()
            }
        }

        NetView {
            id: netView
            anchors.fill: parent
        }

        WalletFrame {
            id: walletFrame
            anchors.bottom: parent.bottom
            anchors.top: title.bottom
            anchors.bottomMargin: x
            opacity: walletDetails.active ? 1 : 0
            visible: opacity !== 0
            width: 250
            x: -border.width
            Behavior on opacity { NumberAnimation { duration: 350 } }
        }
        Row {
            id: buttonBar

            anchors.top: title.bottom
            anchors.margins: 10
            anchors.left: walletFrame.right
            anchors.leftMargin: 10
            opacity: walletFrame.opacity

            spacing: 20
            Button {
                text: qsTr("Send")
                onClicked: sendTransactionPane.start();
            }
            Button {
                text: qsTr("Receive")
            }
        }


        SendTransactionPane {
            id: sendTransactionPane
            anchors.bottom: walletFrame.bottom
            anchors.left: walletFrame.right
            anchors.right: parent.right
            anchors.top: buttonBar.bottom

            opacity: (walletDetails.active && enabled) ? 1 : 0
            visible: opacity !== 0

            Behavior on opacity { NumberAnimation { duration: 350 } }
        }
    }

    AbstractDialog {
        id: createNewWalletDialog
        // standardButtons: StandardButton.Ok | StandardButton.Cancel
        visible: false
        title: "Create New Wallet"

        function showDialog() {
            // init variables here
            privKey.text = ""
            walletName.text = ""
            emptyWalletButton.checked = true
            visible = true
        }

        GridLayout {
            id: diag
            columns: 3
            anchors.fill: parent

            function validate() {
                var ok = false;
                if (privKeyButton.checked) {
                    var typedData = Flowee.identifyString(privKey.text)
                    ok = typedData === Pay.PrivateKey;
                    feedback.checkState = ok ? Qt.Checked : Qt.Unchecked
                } else {
                    ok = true;
                    feedback.checkState = Qt.Unchecked
                }
                buttons.standardButton(DialogButtonBox.Ok).enabled = ok
            }

            Text {
                text: "Please check which kind of wallet you want to create"
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                Layout.columnSpan: 3
            }
            Text { text: "Name:" }
            TextField {
                id: walletName
                Layout.fillWidth: true
            }
            RadioButton {
                id: emptyWalletButton
                Layout.columnSpan: 3
                text: "New, Empty Wallet"
                onCheckedChanged: diag.validate()
            }
            RadioButton {
                id: privKeyButton
                Layout.columnSpan: 3
                text: "Imported Private Key"
                onCheckedChanged: diag.validate()
            }
            Pane { Layout.fillWidth: false }
            TextField {
                id: privKey
                enabled: privKeyButton.checked
                placeholderText: "L5bxhjPeQqVFgCLALiFaJYpptdX6Nf6R9TuKgHaAikcNwg32Q4aL"
                Layout.fillWidth: true
                onTextChanged: diag.validate()
            }
            CheckDelegate {
                id: feedback
                // TODO replace this with a checkbox icon
                Layout.fillWidth: false
            }
            DialogButtonBox {
                id: buttons
                spacing: 6
                Layout.columnSpan: 3
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

                onAccepted: {
                    if (privKeyButton.checked)
                        Flowee.createImportedWallet(privKey.text, walletName.text)
                    else
                        Flowee.createNewWallet(walletName.text);
                    createNewWalletDialog.visible = false
                }
                onRejected: createNewWalletDialog.visible = false

            }
        }
    }
}
