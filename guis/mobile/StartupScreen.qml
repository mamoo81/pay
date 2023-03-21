/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
import QtQuick.Layouts
import "../Flowee" as Flowee

Page {
    id: root
    headerText: qsTr("Welcome!")
    headerButtonVisible: true
    headerButtonText: qsTr("Continue")
    onHeaderButtonClicked: {
        if (!portfolio.current.isUserOwned) {
            var mainView = thePile.get(0);
            mainView.currentIndex = 2 // go to the receive-tab
        }

        thePile.pop();
    }


    Item { // data store for this page.
        Connections {
            // connect to wallet and when its no longe user owned, close this window.
            target: portfolio.current
            function onIsUserOwnedChanged() {
                // transaction seen arriving, this startup screen has fulfilled its purpose.
                thePile.pop();
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: column.height
        clip: true
        Column {
            id: column
            width: parent.width
            spacing: 10

            Row {
                spacing: 20
                x: (column.width - width) / 2

                Rectangle {
                    id: logo
                    width: 100
                    height: 100
                    radius: 50
                    color: mainWindow.floweeBlue
                    Item {
                        // clip the logo only, ignore the text part
                        width: 71
                        height: 71
                        clip: true
                        x: 18
                        y: 22
                        Image {
                            source: "qrc:/FloweePay-light.svg"
                            // ratio: 449 / 77
                            width: height / 77 * 449
                            height: 71
                        }
                    }
                }
                Image {
                    source: "qrc:/bch.svg"
                    y: 10
                    width: 87
                    height: 87
                    smooth: true
                }
            }
            Flowee.Label {
                text: qsTr("Moving the world towards a Bitcoin\u00a0Cash economy")
                wrapMode: Text.WordWrap
                font.italic: true
                horizontalAlignment: Text.AlignHCenter
                width: column.width * 0.8
                x: column.width / 10
            }
            Item { width: 1; height: 20 } // spacer

            Rectangle {
                id: startImportButton
                radius: 20
                height:buttonText.height + 20
                width: buttonText.width + 40
                color: "#178b3a"
                x: (column.width - width) / 2

                Flowee.Label {
                    id: buttonText
                    color: "white"
                    text: qsTr("I already have a wallet")
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: thePile.push("./ImportWalletPage.qml");
                }
            }

            Item { width: 1; height: 5 } // spacer

            Row {
                spacing: 15
                x: (column.width - width) / 2
                Rectangle {
                    width: 50
                    height: 1
                    color: palette.button
                    anchors.verticalCenter: parent.verticalCenter
                }
                Flowee.Label {
                    text: qsTr("OR")
                }
                Rectangle {
                    width: 50
                    height: 1
                    color: palette.button
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Item { width: 1; height: 5 } // spacer

            Flowee.Label {
                id: instructions
                text: qsTr("I want to send funds to my new wallet") + ":"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                width: column.width * 0.8
                x: column.width / 10
            }

            Flowee.QRWidget {
                request: portfolio.current.createPaymentRequest(root)
                width: parent.width
            }

            Item { width: 1; height: 40 } // spacer
        }
    }
}
