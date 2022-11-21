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
import Flowee.org.pay;

ListView {
    id: root

    property string icon: "qrc:/homeButtonIcon" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
    property string  title: qsTr("Home")

    /*
      The structure here is a bit funny.
      But we want to have the entire page scroll in order to show all the transactions we can.
      To avoid a mess of two scroll-areas (of Flickables) we simply make the top part into a
      header of the listview.
     */
    header: ColumnLayout {
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
    }

    model: portfolio.current.transactions
    clip: true
    width: parent.width
    height: contentHeight
    focus: true
    reuseItems: true
    section.property: "grouping"
    section.delegate: Item {
        height: label.height + 15
        width: root.width
        Rectangle {
            color: mainWindow.palette.window
            anchors.fill: parent
        }
        Flowee.Label {
            id: label
            x: 10
            y: 12
            text: section
        }

/* TODO; to-top button
        Image {
            source: "qrc:/back-arrow.svg"
            rotation: 90
            width: 15
            height: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.bottom: parent.bottom
        } */
    }
    delegate: Item {
        id: transactionDelegate
        property var groupType: model.groupType

        width: root.width
        height: 80
        clip: true

        Rectangle {
            width: parent.width - 16
            x: 8
            visible: transactionDelegate.groupType !== Wallet.Ungrouped;
            // we always have the rounded circles, but if we should not see them, we move them out of the screen.
            height: {
                var h = 80
                if (transactionDelegate.groupType !== Wallet.GroupStart)
                    h += 20;
                if (transactionDelegate.groupType !== Wallet.GroupEnd)
                    h += 20;
                return h;
            }
            y: transactionDelegate.groupType === Wallet.GroupStart ? 0 : -20;

            radius: 20
            color: mainWindow.palette.base
            border.width: 1
            border.color: root.palette.highlight
        }

        Item {
            id: ruler
            width: parent.width
            height: 6
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            width: 45
            height: 45
            radius: 22.5
            x: 20
            anchors.verticalCenter: ruler.verticalCenter
        }

        Flowee.Label {
            id: commentLabel
            anchors.bottom: ruler.top
            anchors.right: price.left
            anchors.left: parent.left
            anchors.leftMargin: 80
            clip: true // TODO wordwrap?
            text: {
                var comment = model.comment
                if (comment !== "")
                    return comment;

                if (model.isCoinbase)
                    return qsTr("Miner Reward");
                if (model.isCashFusion)
                    return qsTr("Cash Fusion");
                if (model.fundsIn === 0)
                    return qsTr("Received");
                let diff = model.fundsOut - model.fundsIn;
                if (diff < 0 && diff > -1000) // then the diff is likely just fees.
                    return qsTr("Moved");
                return qsTr("Sent");
            }
        }
        Flowee.Label {
            anchors.top: ruler.bottom
            anchors.left: commentLabel.left
            text: Pay.formatDate(model.date);
        }

        Rectangle {
            id: price
            width: amount.width + 10
            height: amount.height + 10
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: ruler.verticalCenter
            radius: 6

            property int amountBch: model.fundsOut - model.fundsIn
            color: amountBch < 0 ? "#00000000"
                    : (Flowee.useDarkSkin ? "#1d6828" : "#8cff94") // green background
            Flowee.Label {
                id: amount
                text: Fiat.formattedPrice(parent.amountBch, fiatHistory.historicalPrice(model.date));
                anchors.centerIn: parent
                opacity: Math.abs(parent.amountBch) < 2000 ? 0.5 : 1
            }
        }

        Rectangle {
            visible: transactionDelegate.groupType !== Wallet.GroupEnd
                         && transactionDelegate.groupType !== Wallet.Ungrouped;
            anchors.bottom: parent.bottom
            height: 1
            width: parent.width - 16
            x: 8
            color: root.palette.highlight
        }
    }
    Keys.forwardTo: Flowee.ListViewKeyHandler {
        target: root
    }
}