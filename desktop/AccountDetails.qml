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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "widgets" as Widgets
import Flowee.org.pay 1.0

Item {
    id: root
    property QtObject account: portfolio.current

    Connections {
        target: portfolio
        function onCurrentChanged() {
            // The model in this case follows the UI, so we need to have this not very
            // declarative code.
            // When a new model is offered, copy the UI settings into it.
            root.account.secrets.showChangeChain = changeAddresses.checked
            root.account.secrets.showUsedAddresses = usedAddresses.checked
        }
    }


    Label {
        id: walletDetailsLabel
        text: qsTr("Wallet Details")
        font.pointSize: 18
        color: "white"
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Widgets.CloseIcon {
        id: closeIcon
        anchors.bottom: walletDetailsLabel.bottom
        anchors.margins: 6
        anchors.right: parent.right
        onClicked: accountDetails.state = "showTransactions"
    }

    RowLayout {
        id: nameRow
        anchors.top: walletDetailsLabel.bottom
        anchors.topMargin: 20
        x: 20
        width: parent.width - 30

        Label {
            text: qsTr("Name") + ":"
        }
        FloweeTextField {
            id: accountNameEdit
            text: root.account.name
            onTextEdited: root.account.name = text
            Layout.fillWidth: true
            focus: true
        }
    }

    Flickable {
        id: scrollablePage
        anchors.top: nameRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        clip: true

        contentHeight:  detailsPane.height + 10 + addressesList.height + 10

        ColumnLayout {
            id: detailsPane
            spacing: 10
            width: parent.width - 20
            x: 10

            Label {
                text: qsTr("Sync Status") + ": " + syncIndicator.accountBlockHeight + " / " + syncIndicator.globalPos
            }
            Label {
                id: walletType
                font.italic: true
                text: {
                    if (root.account.isSingleAddressAccount)
                        return qsTr("This wallet is a single-address wallet.")

                    if (root.account.isHDWallet)
                        return qsTr("This wallet is based on a HD seed mnemonic")

                     // ok we only have one more type so far, so this is rather simple...
                    return qsTr("This wallet is a simple multiple-address wallet.")
                }
            }
    /* TODO, features to add;
            Label {
                text: qsTr("Security:")
                Layout.columnSpan: 3
                font.italic: true
            }

            FloweeCheckBox {
                id: schnorr
                checked: true
                text: qsTr("Use Schnorr signatures");
            }
            FloweeCheckBox {
                id: pinToTray
                checked: false
                text: qsTr("Pin to Pay");
            }
            FloweeCheckBox {
                id: pinToOpen
                checked: false
                text: qsTr("Pin to Open");
            }
            FloweeCheckBox {
                id: syncOnStart
                checked: false
                text: qsTr("Sync on Start");
            }
            FloweeCheckBox {
                id: useIndexServer
                checked: false
                text: qsTr("Use Indexing Server");
            }
    */
        }

        FloweeGroupBox {
            id: addressesList
            width: parent.width
            anchors.top: detailsPane.bottom
            anchors.topMargin: 10
            title: qsTr("Address List")
            collapsed: !root.account.isSingleAddressAccount

            FloweeCheckBox {
                id: changeAddresses
                text: qsTr("Change Addresses")
                visible: root.account.isHDWallet
                onClicked: root.account.secrets.showChangeChain = checked
                tooltipText: qsTr("Switches between normal addresses or Bitcoin addresses used for coins that are change.")
            }
            FloweeCheckBox {
                id: usedAddresses
                text: qsTr("Used Addresses");
                visible: !root.account.isSingleAddressAccount
                onClicked: root.account.secrets.showUsedAddresses = checked
                tooltipText: qsTr("Switches between unused and used bitcoin addresses")
            }
            ListView {
                model: root.account.secrets
                Layout.fillWidth: true
                implicitHeight: root.account.isSingleAddressAccount ? contentHeight : scrollablePage.height / 10 * 7
                clip: true
                delegate: Rectangle {
                    id: delegateRoot
                    color: (index % 2) == 0 ? mainWindow.palette.base : mainWindow.palette.alternateBase
                    width: ListView.view.width
                    height: addressLabel.height + 10 + amountLabel.height + 10

                    Label {
                        text: hdIndex
                        anchors.baseline: addressLabel.baseline
                        anchors.right: addressLabel.left
                        anchors.rightMargin: 10
                        visible: root.account.isHDWallet
                    }

                    LabelWithClipboard {
                        id: addressLabel
                        y: 5
                        x: root.account.isHDWallet ? 50 : 0
                        text: address
                    }

                    BitcoinAmountLabel {
                        id: amountLabel
                        value: balance
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12
                    }
                    Label {
                        id: coinCountLabel
                        anchors.left: parent.left
                        anchors.leftMargin: delegateRoot.width / 4
                        anchors.baseline: amountLabel.baseline
                        text: qsTr("Coins: %1 / %2").arg(numCoins).arg(historicalCoinCount)
                    }
                    Label {
                        id: schnorrIndicator
                        anchors.left: coinCountLabel.right
                        anchors.leftMargin: 10
                        anchors.baseline: amountLabel.baseline
                        visible: usedSchnorr
                        text: "ⓢ"
                        MouseArea {
                            id: mousy
                            anchors.fill: parent
                            anchors.margins: -10
                            hoverEnabled: true

                            ToolTip {
                                parent: schnorrIndicator
                                text: qsTr("Signed with Schnorr signatures in the past")
                                delay: 700
                                visible: mousy.containsMouse
                            }
                        }
                    }

                    ConfigItem {
                        id: button
                        anchors.right: parent.right
                        wide: true
                        y: 10
                        contextMenu: Menu {
                            id: contextMenu
                            MenuItem {
                                text: qsTr("Copy Address")
                                onTriggered: Flowee.copyToClipboard(address)
                            }
                            MenuItem {
                                text: qsTr("Copy Private Key")
                                onTriggered: Flowee.copyToClipboard(privatekey)
                            }
                            /*
                                MenuItem {
                                    text: qsTr("Move to New Wallet")
                                    onTriggered: ;
                                } */
                        }
                    }
                }
            }
        }
        FloweeGroupBox {
            id: hdDetails
            width: parent.width
            anchors.top: addressesList.bottom
            anchors.topMargin: 10
            title: qsTr("Backup details")
            visible: root.account.isHDWallet
            collapsed: true

            Item {
                width: parent.width
                implicitHeight: grid.height + helpText.height + warningText.height + 20
                GridLayout {
                    id: grid
                    width: parent.width
                    columns: 2
                    Label {
                        text: qsTr("Seed Phrase") + ":"
                    }
                    TextArea {
                        readOnly: true
                        text: root.account.mnemonic
                        Layout.fillWidth: true
                        selectByMouse: true
                        mouseSelectionMode: TextEdit.SelectWords
                        wrapMode: TextEdit.WordWrap
                        padding: 0
                    }
                    Label {
                        text: qsTr("Derivation") + ":"
                    }
                    LabelWithClipboard {
                        text: root.account.hdDerivationPath
                    }
                }

                Label {
                    id: helpText
                    width: parent.width
                    anchors.top: grid.bottom
                    anchors.topMargin: 10
                    text: qsTr("Please save the seed phrase on paper, in the right order, with the derivation path. This seed will allow you to recover your wallet in case of computer failure.")
                    textFormat: Text.StyledText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Label {
                    id: warningText
                    width: parent.width
                    anchors.top: helpText.bottom
                    anchors.topMargin: 10
                    text: qsTr("<b>Important</b>: Never share your seed with others!")
                    textFormat: Text.StyledText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }
    }
}
