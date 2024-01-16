/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2024 Tom Zander <tom@flowee.org>
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

ListView {
    id: root
    required property QtObject account;

    model: root.account.secrets
    property bool showHdIndex: true
    Item {
        // non-layoutable items.

        QQC2.Popup {
            id: qrPopup
            width: 270
            height: 270
            x: (root.width - width) / 2
            y: 100
            modal: true
            closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent
            background: Rectangle {
                color: palette.light
                border.color: palette.midlight
                border.width: 1
                radius: 5
            }
            QRWidget {
                id: seedQr
                qrSize: 250
                textVisible: false
                useRawString: true
                anchors.centerIn: parent
            }
        }
    }

    header: Item {
        width: root.width
        implicitHeight: contentColumn.implicitHeight + 16
        Rectangle {
            color: palette.alternateBase
            anchors {
                fill: parent
                leftMargin: -10
                rightMargin: -10
            }
        }

        Column {
            id: contentColumn
            width: root.width
            y: 4
            spacing: 6
            Label {
                width: parent.width
                font.italic: true
                text: qsTr("Explanation")
                font.weight: 600
            }
            Label {
                width: parent.width
                font.italic: true
                text: qsTr("Coins a / b\n a) active coin-count.\n b) historical coin-count.");
            }
        }
    }

    delegate: Item {

        id: delegateRoot
        width: ListView.view.width
        height: addressLabel.height + 6 + amountLabel.height + 6 + (lineCount === 3 ? coinCountLabel.height + 6: 0) + 12

        property int lineCount: {
            var x = coinCountLabel.x + coinCountLabel.width + 10
            if (schnorrRect.visible)
                x += schnorrRect.width;
            if (delegateRoot.width < x + amountLabel.width) // they don't fit.
                return 3;
            return 2;
        }

        Rectangle {
            color: (index % 2) == 0 ? palette.base : palette.alternateBase
            anchors {
                fill: parent
                leftMargin: -10
                rightMargin: -10
            }
        }
        Label {
            text: hdIndex
            anchors.baseline: addressLabel.baseline
            anchors.right: addressLabel.left
            anchors.rightMargin: 10
            visible: root.showHdIndex && root.account.isHDWallet
        }

        LabelWithClipboard {
            id: addressLabel
            y: 5
            x: root.account.isHDWallet ? 50 : 5
            anchors.bottomMargin: 6
            text: address
            anchors.left: parent.left
            anchors.leftMargin: root.showHdIndex ? 40 : 0
            anchors.right: hamburgerMenu.left
            anchors.rightMargin: 6
            menuText: qsTr("Copy Address")
        }

        Item {
            id: hamburgerMenu
            width: 12
            height: column.height
            anchors.right: parent.right
            anchors.verticalCenter: addressLabel.verticalCenter
            Column {
                id: column
                spacing: 3
                y: 1 // move the column down to account for the anti-alias line of the rectangle below

                Repeater { // hamburger
                    model: 3
                    delegate: Rectangle {
                        color: palette.windowText
                        width: 12
                        height: 3
                        radius: 2
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -10
                hoverEnabled: true // to make sure we eat them and avoid the hover feedback.
                acceptedButtons: Qt.RightButton | Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                QQC2.Menu {
                    id: ourMenu
                    QQC2.Action {
                        text: qsTr("Copy Address")
                        onTriggered: Pay.copyToClipboard(Pay.chainPrefix + address)
                    }
                    QQC2.Action {
                        text: qsTr("QR of Address")
                        onTriggered: {
                            seedQr.qrText = Pay.chainPrefix + address
                            qrPopup.open();
                        }
                    }
                    QQC2.Action {
                        text: qsTr("Copy Private Key")
                        onTriggered: Pay.copyToClipboard(privatekey)
                    }
                    QQC2.Action {
                        text: qsTr("QR of Private Key")
                        onTriggered: {
                            seedQr.qrText = privatekey
                            qrPopup.open();
                        }
                    }
                }
                onClicked: ourMenu.popup()
            }
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
            x: root.width > 400 ? delegateRoot.width / 4 : 35;
            y: {
                if (delegateRoot.lineCount === 2) {
                    // then this is on the same line as the amountLabel
                    return amountLabel.y + amountLabel.baselineOffset - baselineOffset;
                }
                // then we place ourselves below the address
                return addressLabel.y + addressLabel.height + 6
            }
            text: qsTr("Coins: %1 / %2").arg(numCoins).arg(historicalCoinCount)
        }
        Rectangle {
            id: schnorrRect
            visible: usedSchnorr
            width: schnorrIndicator.height
            height: width
            radius: width / 2
            anchors.left: coinCountLabel.right
            anchors.leftMargin: 6
            anchors.verticalCenter: coinCountLabel.verticalCenter
            anchors.margins: -4
            color: Pay.useDarkSkin ? "#838383" : "#ccc"
            Label {
                id: schnorrIndicator
                anchors.centerIn: parent
                font.bold: true
                text: "S"
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -10
                    hoverEnabled: true
                    onClicked: QQC2.ToolTip.show(qsTr("Signed with Schnorr signatures in the past"), 7500);
                }
            }
        }
    }
}
