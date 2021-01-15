/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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


Pane {
    id: root
    visible: account !== null

    property QtObject account: isLoading ? null : portfolio.current

    Rectangle {
        id: accountHeader
        width: parent.width
        height: column.height + 40
        radius: 10

        Column {
            id: column
            width: parent.width
            y: 20

            ConfigItem {
                id: accountConfigIcon
                anchors.right: parent.right
                anchors.rightMargin: 20

                color: mainWindow.palette.text

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -10
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: contextMenu.popup()

                    Menu {
                        id: contextMenu
                        MenuItem {
                            text: qsTr("Details")
                            onTriggered: accountDetailsDialog.source = "./AccountDetails.qml";
                        }
                        /*
                        MenuItem {
                            text: qsTr("Rename")
                        } */
                    }
                }
            }

            GridLayout {
                id: valueGrid
                columns: 2
                anchors.horizontalCenter: parent.horizontalCenter
                BitcoinAmountLabel {
                    id: balance
                    value: root.account === null ? 0 :
                               root.account.balanceConfirmed + root.account.balanceUnconfirmed
                    colorize: false
                    textColor: mainWindow.palette.text
                    fontPtSize: root.font.pointSize * 2
                    includeUnit: false
                    Layout.alignment: Qt.AlignBaseline
                }
                Label {
                    text: Flowee.unitName
                    font.pointSize: root.font.pointSize * 1.6
                    Layout.alignment: Qt.AlignBaseline
                }
                BitcoinAmountLabel {
                    id: balanceImmature
                    value: root.account === null ? 0 : root.account.balanceImmature
                    colorize: false
                    textColor: mainWindow.palette.text
                    fontPtSize: root.font.pointSize * 2
                    includeUnit: false
                    Layout.alignment: Qt.AlignBaseline
                    visible: value > 0
                }
                Label {
                    visible: balanceImmature.visible
                    text: Flowee.unitName
                    font.pointSize: root.font.pointSize * 1.6
                    Layout.alignment: Qt.AlignBaseline
                }
                Label {
                    text: "0.00"
                    font.pointSize: root.font.pointSize * 2
                    Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                    visible: Flowee.isMainChain
                }
                Label {
                    text: "Euro"
                    font.pointSize: root.font.pointSize * 1.6
                    Layout.alignment: Qt.AlignBaseline
                    visible: Flowee.isMainChain
                }
            }
            Item { width: 1; height: 20 } // spacer

            Label {
                id: accountName
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.account === null ? "" : root.account.name
                font.bold: true
            }
            Item { width: 1; height: 10 } // spacer
            SyncIndicator {
                accountBlockHeight: root.account === null ? 0 : root.account.lastBlockSynched
                anchors.horizontalCenter: parent.horizontalCenter
                visible: globalPos !== accountBlockHeight
            }

            Item { width: 1; height: 10 } // spacer

            RowLayout {
                id: buttons
                anchors.horizontalCenter: parent.horizontalCenter
                Button2 {
                    text: qsTr("&Send...")
                    onClicked: {
                        stack.push("./SendTransactionPane.qml")
                        stack.focus = true
                    }
                    enabled: stack.depth === 1 && root.account !== null &&
                             root.account.balanceUnconfirmed + root.account.balanceConfirmed > 0
                }
                Button2 {
                    text: qsTr("&Receive...")
                    enabled: stack.depth === 1
                    onClicked: {
                        stack.push("./ReceiveTransactionPane.qml")
                    }
                }
            }
        }

        Image {
            anchors.top: column.top
            anchors.left: column.left
            anchors.margins: 30
            width: 30
            height: 30
            smooth: true
            source: (root.account !== null && root.account.isDefaultWallet)
                    ? "qrc:///heart-red.svg" : "qrc:///heart.svg"

            MouseArea {
                anchors.fill: parent
                onClicked: root.account.isDefaultWallet = !root.account.isDefaultWallet
            }
        }

        gradient: Gradient {
            GradientStop {
                position: 0.1
                color: "#48a35c"
            }
            GradientStop {
                position: 1
                color: root.palette.base
            }
        }
    }

    StackView {
        id: stack
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: accountHeader.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 6
        initialItem: ListView {
            model: root.account === null ? 0 : root.account.transactions
            clip: true
            delegate: WalletTransaction { width: stack.width }
        }

        // when top child sets its own visibility to false, POP it from the stack
        onCurrentItemChanged: {
            currentItem.focus = true
            autoRemover.target = currentItem
        }
        Connections {
            id: autoRemover
            function onVisibleChanged() { stack.pop(); }
        }
    }

    Flow {
        width: parent.width
        anchors.bottom: parent.bottom
        spacing: 10
        Repeater {
            model: root.account.paymentRequests
            delegate: Rectangle {
                width: 70
                height: width
                radius: 25
                border.width: 6
                border.color: {
                    var state = modelData.state;
                    if (state === PaymentRequest.Unpaid)
                        return "#888888"
                    if (state === PaymentRequest.PaymentSeen)
                        return "yellow"
                    if (state === PaymentRequest.DoubleSpentSeen)
                        return "red"
                    // in all other cases:
                    return "green"
                }

                // don't show the one we are editing
                visible: modelData.saveState === PaymentRequest.Remembered

                Text {
                    anchors.centerIn: parent
                    text: modelData.message
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: paymentContextMenu.popup()
                    Menu {
                        id: paymentContextMenu
                        MenuItem {
                            text: qsTr("Delete")
                            onTriggered: modelData.forgetPaymentRequest()
                        }
                    }
                }
            }
        }
    }
}
