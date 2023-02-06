/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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
import QtQuick.Layouts
import "../Flowee" as Flowee

QQC2.Popup {
    id: root
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent
    height: columnLayout.height + 10
    focus: true // to allow Escape to close it

    property QtObject selectedAccount: portfolio.current

    Connections {
        target: menuOverlay
        function onOpenChanged() { root.close(); }
    }

    background: Rectangle {
        color: root.palette.base
        border.color: root.palette.highlight
        border.width: 1
        radius: 5
    }

    ColumnLayout {
        id: columnLayout
        width: parent.width

        Flowee.Label {
            text: qsTr("Your Wallets")
            font.bold: true
        }

        Repeater { // portfolio holds all our accounts
            width: parent.width
            model: portfolio.accounts
            delegate: Item {
                width: columnLayout.width
                height: accountName.height + lastActive.height + 6 * 3
                Rectangle {
                    color: root.palette.button
                    radius: 5
                    anchors.fill: parent
                    visible: modelData === root.selectedAccount
                }
                Flowee.Label {
                    id: accountName
                    y: 6
                    x: 6
                    text: modelData.name
                }
                Flowee.Label {
                    id: fiat
                    anchors.top: accountName.top
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    text: Fiat.formattedPrice(modelData.balanceConfirmed + modelData.balanceUnconfirmed, Fiat.price)
                }
                Flowee.Label {
                    id: lastActive
                    anchors.top: accountName.bottom
                    anchors.left: accountName.left
                    text: qsTr("last active") + ": " + Pay.formatDate(modelData.lastMinedTransaction)
                    font.pixelSize: root.font.pixelSize * 0.8
                    font.bold: false
                }
                Flowee.BitcoinAmountLabel {
                    anchors.right: fiat.right
                    anchors.top: lastActive.top
                    font.pixelSize: lastActive.font.pixelSize
                    showFiat: false
                    value: modelData.balanceConfirmed + modelData.balanceUnconfirmed
                    colorize: false
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.selectedAccount = modelData
                        root.close();
                    }
                }
            }
        }
    }
}
