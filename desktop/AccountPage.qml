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


ScrollView {
    id: root
    contentHeight: {
        return walletHeader.height + 10
    }
    visible: wallet !== null

    property QtObject wallet: isLoading ? null : wallets.current

    Rectangle {
        id: walletHeader
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
                anchors.rightMargin: 10

                color: mainWindow.palette.text
            }

            GridLayout {
                id: valueGrid
                columns: 2
                anchors.horizontalCenter: parent.horizontalCenter
                BitcoinAmountLabel {
                    id: balance
                    value: root.wallet === null ? 0 : root.wallet.balance
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
                Label {
                    text: "0.00"
                    font.pointSize: root.font.pointSize * 2
                    Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                }
                Label {
                    text: "Euro"
                    font.pointSize: root.font.pointSize * 1.6
                    Layout.alignment: Qt.AlignBaseline
                }
            }
            Item { width: 1; height: 20 } // spacer

            Label {
                id: accountName
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.wallet === null ? "" : root.wallet.name
                font.italic: true
            }

            Item { width: 1; height: 20 } // spacer

            RowLayout {
                id: buttons
                anchors.horizontalCenter: parent.horizontalCenter
                Button2 {
                    text: qsTr("&Send...")
                    onClicked: {
                        sendTransactionPane.reset();
                        sendTransactionPane.visible = true;
                        sendTransactionPane.focus = true;
                    }
                    enabled: !sendTransactionPane.visible && root.wallet !== null && root.wallet.balance > 0
                }
                Button2 {
                    text: qsTr("&Receive...")
                    enabled: !sendTransactionPane.visible
                }
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

    SendTransactionPane {
        id: sendTransactionPane
        width: parent.width
        anchors.top: walletHeader.bottom
        anchors.topMargin: 10
        visible: false
    }
}
