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
    property bool showTotalBalance: true
    property bool showEncryptedAccounts: true

    Connections {
        target: menuOverlay
        function onOpenChanged() { root.close(); }
    }

    background: Rectangle {
        color: palette.light
        border.color: palette.midlight
        border.width: 1
        radius: 5
    }

    Column {
        id: columnLayout
        width: parent.width

        Flowee.Label {
            text: qsTr("Your Wallets")
            font.bold: true
            visible: portfolio.accounts.length > 1
        }

        Repeater { // portfolio holds all our accounts
            width: parent.width
            model: {
                var accounts = portfolio.accounts
                if (root.showEncryptedAccounts === false) {
                    // then we filter them out here.
                    let copy = accounts;
                    accounts = [];
                    for (let account of copy) {
                        if (account.isDecrypted || !account.needsPinToOpen)
                            accounts.push(account);
                    }
                }
                return accounts;
            }
            delegate: Item {
                width: columnLayout.width
                height: {
                    var h = accountName.height + lastActive.height + 6 * 3;
                    // if the width is not enough to fit, the bchamount moves
                    // to its own line.
                    if (6 + lastActive.width + 6 + bchAmountLabel.width + 6 > width)
                        h += bchAmountLabel.height + 6
                    return h;
                }
                Rectangle {
                    id: selectedItemIndicator
                    visible: modelData === root.selectedAccount
                    anchors.fill: parent
                    color: palette.highlight
                    opacity: 0.15
                }
                Rectangle {
                    height: parent.height
                    width: 3
                    color: palette.highlight
                    visible: selectedItemIndicator.visible
                }

                Image {
                    id: lockIcon
                    x: 6
                    width: 12
                    height: 12
                    anchors.verticalCenter: parent.verticalCenter
                    visible: modelData.needsPinToPay
                    source: {
                        if (visible)
                            return "qrc:/lock" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
                        return "";
                    }
                }

                Flowee.Label {
                    id: accountName
                    y: 6
                    x: lockIcon.visible ? 24: 6
                    text: modelData.name
                }
                Flowee.Label {
                    id: fiat
                    anchors.top: accountName.top
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    text: Fiat.formattedPrice(modelData.balanceConfirmed + modelData.balanceUnconfirmed, Fiat.price)
                    visible: modelData.countBalance && (modelData.isDecrypted || !modelData.needsPinToOpen)
                }
                Flowee.Label {
                    id: lastActive
                    anchors.top: accountName.bottom
                    anchors.topMargin: 6
                    anchors.left: accountName.left
                    text: qsTr("last active") + ": " + Pay.formatDate(modelData.lastMinedTransaction)
                    font.pixelSize: root.font.pixelSize * 0.8
                    visible: modelData.isDecrypted || !modelData.needsPinToOpen
                }
                Flowee.Label {
                    id: lockStatus
                    anchors.top: accountName.bottom
                    anchors.topMargin: 6
                    anchors.left: accountName.left
                    text: qsTr("Needs PIN to open")
                    font.pixelSize: root.font.pixelSize * 0.8
                    visible: !lastActive.visible
                }
                Flowee.BitcoinAmountLabel {
                    id: bchAmountLabel
                    anchors.right: fiat.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    font.pixelSize: lastActive.font.pixelSize
                    showFiat: false
                    value: modelData.balanceConfirmed + modelData.balanceUnconfirmed
                    colorize: false
                    visible: fiat.visible
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

        Item {
            width: parent.width
            opacity: 0.8 // less bright || dark text
            visible: !portfolio.singleAccountSetup && root.showTotalBalance
            implicitHeight: 5 + totalLabel.implicitHeight +
                             (totalLabel.width + 6 + 6 + totalAmount.width + 6 > width
                                ? totalAmount.implicitHeight : 0)
            Flowee.Label {
                id: totalLabel
                x: 6
                y: 5
                text: qsTr("Balance Total") + ":"
                font.pixelSize: root.font.pixelSize * 0.8
            }
            Flowee.BitcoinAmountLabel {
                id: totalAmount
                value: portfolio.totalBalance
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                font.pixelSize: root.font.pixelSize * 0.8
                colorize: false
            }
        }
    }
}
