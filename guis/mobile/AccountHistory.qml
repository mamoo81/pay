/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 2.15
import "../Flowee" as Flowee

Flickable {
    id: accountHistory
    property string icon: "qrc:/homeButtonIcon" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
    property string  title: qsTr("Home")

    contentHeight: column.height + 20
    clip: true
    Rectangle {
        // background color "base", to indicate these are widgets here.
        anchors.fill: parent
        color: mainWindow.palette.base
        ColumnLayout {
            id: column
            spacing: 20
            width: parent.width - 20
            x: 10
            y: 10

            Item {
                id: smallAccountSelector
                width: parent.width
                height: walletSelector.height
                z: 10

                property bool open: false

                visible: {
                    // if there is only an initial, not user-owned wallet, there is nothing to show here.
                    return portfolio.current.isUserOwned
                }

                Rectangle {
                    id: walletSelector
                    height: currentWalletName.height + 14
                    width: parent.width + 5
                    color: "#00000000"
                    border.color: mainWindow.palette.button
                    border.width: hasMultipleWallets ? 0.7 : 0
                    x: -5

                    property bool hasMultipleWallets: portfolio.accounts.length > 1

                    Flowee.Label {
                        x: 5
                        id: currentWalletName
                        text: portfolio.current.name
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Flowee.ArrowPoint {
                        id: arrowPoint
                        visible: parent.hasMultipleWallets
                        color: mainWindow.palette.text
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        rotation: 90
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.hasMultipleWallets
                        onClicked: {
                            smallAccountSelector.open = !smallAccountSelector.open
                            // move focus so the 'back' button will close it.
                            extraWallets.forceActiveFocus();
                        }
                    }
                }

                Rectangle {
                    id: extraWallets
                    width: parent.width
                    y: walletSelector.height
                    height: parent.open ? columnLayout.height + 20 : 0
                    color: mainWindow.palette.base
                    Behavior on height {
                        NumberAnimation {
                            duration: 200
                        }
                    }

                    clip: true
                    visible: height > 0
                    focus: true
                    ColumnLayout {
                        y: 10
                        id: columnLayout
                        width: parent.width
                        Item {
                            // horizontal divider.
                            width: parent.width
                            height: 1
                            Rectangle {
                                height: 1
                                width: parent.width / 10 * 7
                                x: (parent.width - width) / 2 // center in column
                                color: mainWindow.palette.highlight
                            }
                        }

                        Repeater { // portfolio holds all our accounts
                            width: parent.width
                            model: portfolio.accounts
                            delegate: Item {
                                width: extraWallets.width
                                height: accountName.height * 2 + 12
                                Rectangle {
                                    color: mainWindow.palette.button
                                    anchors.fill: parent
                                    visible: modelData === portfolio.current
                                }
                                Flowee.Label {
                                    id: accountName
                                    y: 6
                                    text: modelData.name
                                }
                                Flowee.Label {
                                    id: lastActive
                                    anchors.top: accountName.bottom
                                    text: qsTr("last active") + ": "
                                    font.pointSize: mainWindow.font.pointSize * 0.8
                                    font.bold: false
                                }
                                Flowee.Label {
                                    anchors.top: lastActive.top
                                    anchors.left: lastActive.right
                                    text: Pay.formatDate(modelData.lastMinedTransaction)
                                    font.pointSize: mainWindow.font.pointSize * 0.8
                                    font.bold: false
                                }
                                Flowee.BitcoinAmountLabel {
                                    anchors.right: parent.right
                                    anchors.top: accountName.top
                                    showFiat: true
                                    value: modelData.balanceConfirmed + modelData.balanceUnconfirmed
                                    colorize: false
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        portfolio.current = modelData
                                        smallAccountSelector.open = false
                                    }
                                }
                            }
                        }
                        Item {
                            // horizontal divider.
                            width: parent.width
                            height: 1
                            Rectangle {
                                height: 1
                                width: parent.width / 10 * 7
                                x: (parent.width - width) / 2 // center in column
                                color: mainWindow.palette.highlight
                            }
                        }
                    }

                    Keys.onPressed: (event)=> {
                        if (event.key === Qt.Key_Back) {
                            event.accepted = true;
                            smallAccountSelector.open = false
                        }
                    }
                }
            }
            Flowee.BitcoinAmountLabel {
                opacity: Pay.hideBalance ? 0.2 : 1
                value: {
                    if (Pay.hideBalance)
                        return 88888888;
                    return portfolio.current.balanceConfirmed + portfolio.current.balanceUnconfirmed
                }
                colorize: false
            }

            Row {
                width: parent.width
                height: 60
                IconButton {
                    width: parent.width / 3
                    text: qsTr("Send Money")
                }
                IconButton {
                    width: parent.width / 3
                    text: qsTr("Scheduled")
                }
                IconButton {
                    width: parent.width / 3
                    text: qsTr("Receive")
                }
            }

            /* TODO
              "Is archive" / "Unrchive""

              Is Encryopted / Decrypt
             */
            ListView {
                id: activityVIew
                model: portfolio.current.transactions
                clip: true
                width: parent.width
                delegate: Item {
                    width: activityVIew.width
                    height: 40
                }

                ScrollBar.vertical: Flowee.ScrollThumb {
                    id: thumb
                    minimumSize: 20 / activityView.height
                    visible: size < 0.9
                    preview: Rectangle {
                        width: label.width + 12
                        height: label.height + 12
                        radius: 5
                        color: label.palette.dark
                        QQC2.Label {
                            id: label
                            anchors.centerIn: parent
                            color: palette.light
                            text: isLoading || activityView.model === null ? "" : activityView.model.dateForItem(thumb.position);
                        }
                    }
                }
                Keys.forwardTo: Flowee.ListViewKeyHandler {
                    target: activityView
                }

            }
        }
    }
}
