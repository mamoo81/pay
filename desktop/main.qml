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

import "./ControlColors.js" as ControlColors

ApplicationWindow {
    id: mainWindow
    visible: true
    width: Flowee.windowWidth === -1 ? 750 : Flowee.windowWidth
    height: Flowee.windowHeight === -1 ? 500 : Flowee.windowHeight
    minimumWidth: 600
    minimumHeight: 500
    title: "Flowee Pay"

    onWidthChanged: Flowee.windowWidth = width
    onHeightChanged: Flowee.windowHeight = height
    onVisibleChanged: if (visible) ControlColors.applySkin(mainWindow)

    property bool isLoading: typeof portfolio === "undefined";

    onIsLoadingChanged: {
        if (!isLoading) {
            if (portfolio.current === null) { // move to 'receive' tab if there is no wallet.
                tabbar.currentIndex = 2;
            }
            else {
                tabbar.currentIndex = 0;
            }

            receivePane.source = "./ReceiveTransactionPane.qml"
        }
    }

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"

    header: Rectangle {
        color: Flowee.useDarkSkin ? "#00000000" : mainWindow.floweeBlue
        width: parent.width
        height: {
            var h = mainWindow.height;
            if (h > 700)
                return 120;
            return h / 700 * 120;
        }

        Rectangle {
            color: mainWindow.floweeBlue
            opacity: Flowee.useDarkSkin ? 1 : 0

            width: parent.height / 5 * 4
            height: width
            radius: width / 2
            x: 2
            y: 8
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

        Image {
            anchors.verticalCenter: parent.verticalCenter
            x: 20
            smooth: true
            source: "FloweePay-light.svg"
            // ratio: 77 / 449
            height: (parent.height - 20) * 7 / 10
            width: height * 449 / 77
        }
        Text {
            y: parent.height / 3 * 2 - height
            anchors.right: parent.right
            anchors.rightMargin: 30
            text: "BCH: " + "€1000"
            visible: Flowee.isMainChain
            font.pixelSize: 18
            color: "white"
        }
    }

    Pane {
        id: mainScreen
        anchors.fill: parent
        focus: true

        Item {
            id: tabbedPane
            height: parent.height
            width: parent.width * 65 / 100
            anchors.right: parent.right

            Rectangle {
                anchors.fill: parent
                opacity: 0.2
                anchors.margins: -5
                radius: 5
                color: Flowee.useDarkSkin ? "black" : "white"
            }
            FloweeTabBar {
                id: tabbar
                height: activityButton.height
                width: parent.width

                PayTabButton {
                    id: activityButton
                    text: qsTr("Activity")
                    width: parent.width / 4
                    onActivated: tabbar.currentIndex = getIndex()
                }
                PayTabButton {
                    text: qsTr("Send")
                    width: parent.width / 4
                    onActivated: tabbar.currentIndex = getIndex()
                }
                PayTabButton {
                    text: qsTr("Receive")
                    width: parent.width / 4
                    onActivated: tabbar.currentIndex = getIndex()
                }
                PayTabButton {
                    text: qsTr("Settings")
                    width: parent.width / 4
                    onActivated: tabbar.currentIndex = getIndex()
                }
            }
            StackLayout {
                id: stack
                width: parent.width
                anchors.top: tabbar.bottom
                anchors.bottom: parent.bottom
                currentIndex: tabbar.currentIndex

                Pane {
                    ListView {
                        id: activityView
                        model: isLoading || portfolio.current === null ? 0 : portfolio.current.transactions
                        clip: true
                        delegate: WalletTransaction { width: activityView.width }
                        anchors.fill: parent
                    }
                }
                SendTransactionPane { }
                Loader { id: receivePane }
                Pane {
                    id: settingsPane

                    GridLayout {
                        columns: 2

                        CheckBox {
                            text: "Night mode:"
                            checked: Flowee.useDarkSkin
                            onClicked: {
                                Flowee.useDarkSkin = !Flowee.useDarkSkin
                                ControlColors.applySkin(mainWindow);
                            }
                        }

                        /*
                          TODO:
                          unit-name selection
                        */
                    }
                }
            }
        }

        Flickable {
            id: overviewPane
            width: parent.width - tabbedPane.width
            height: parent.height
            contentWidth: leftColumn.width
            contentHeight: leftColumn.height
            flickableDirection: Flickable.VerticalFlick

            Column {
                id: leftColumn
                y: 40
                width: overviewPane.width - 60

                Label {
                    text: qsTr("Balance");
                    height: implicitHeight / 10 * 7
                }
                BitcoinAmountLabel {
                    id: balance
                    value: {
                        if (isLoading)
                            return 0;
                        var account = portfolio.current;
                        if (account === null)
                            return 0;
                        return account.balanceConfirmed + account.balanceUnconfirmed
                    }
                    colorize: false
                    textColor: mainWindow.palette.text
                    fontPtSize: mainWindow.font.pointSize * 3
                }
                Label {
                    text: qsTr("0.00 EUR"); // TODO
                    visible: Flowee.isMainChain
                    opacity: 0.3
                }
                Item { // spacer
                    width: 10
                    height: 20
                }
                Label {
                    id: totalBalance
                    visible: mainWindow.isLoading || portfolio.accounts.length > 1
                    text: qsTr("Total balance");
                    height: implicitHeight / 10 * 9
                }
                BitcoinAmountLabel {
                    value: {
                        if (isLoading)
                            return 0;
                        return 1; // TODO
                        // return portfolio.totalBalance
                    }
                    visible: totalBalance.visible
                    colorize: false
                    fontPtSize: mainWindow.font.pointSize * 2
                }
                Label {
                    text: qsTr("0.00 EUR"); // TODO
                    visible: totalBalance.visible && Flowee.isMainChain
                    opacity: 0.3
                }
                Item { // spacer
                    visible: totalBalance.visible
                    width: 10
                    height: 40
                }
                Label {
                    text: qsTr("Network status")
                    opacity: 0.3
                }
                SyncIndicator {
                    accountBlockHeight: isLoading || portfolio.current === null ? 0 : portfolio.current.lastBlockSynched
                    visible: !isLoading && portfolio.current !== null && globalPos !== accountBlockHeight
                }
                Item { // spacer
                    width: 10
                    height: 60
                }
                Repeater {
                    width: parent.width
                    // Layout.fillWidth: true
                    // Layout.fillHeight: true
                    model: {
                        if (typeof portfolio === "undefined")
                            return 0;
                        return portfolio.accounts;
                    }
                    delegate: Item {
                        width: leftColumn.width
                        height: name.height + 20 + 30

                        Rectangle {
                            property bool hover: false
                            anchors.fill: parent
                            anchors.margins: 15
                            radius: 10
                            color: "#00000000" // transparant

                            border.width: 3
                            border.color: {
                                if (portfolio.current === modelData)
                                    return mainWindow.floweeGreen
                                if (hover)
                                    return mainWindow.floweeSalmon;
                                return Flowee.useDarkSkin ? "#EEE" : mainWindow.floweeBlue
                            }

                            Label {
                                id: name
                                font.bold: true
                                x: 10
                                y: 10
                                text: modelData.name
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: portfolio.current = modelData
                                onEntered: parent.hover = true
                                onExited: parent.hover = false
                            }
                        }
                    }
                }
                Item { // spacer
                    width: 10
                    height: 40
                }

                Rectangle {
                    color: mainWindow.floweeGreen
                    radius: 10
                    width: balance.width
                    height: buttonLabel.height + 30
                    // anchors.bottom: parent.bottom
                    // anchors.bottomMargin: 30
                    Text {
                        id: buttonLabel
                        anchors.centerIn: parent
                        width: parent.width - 20
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        text: "Import your Bitcoin Cash wallet"
                    }
                }
                Item { // spacer
                    width: 10
                    height: 40
                }
            }
        }

        Rectangle {
            id: splashScreen
            color: "#FFFFFF"
            anchors.fill: parent
            Text {
                text: "Preparing..."
                anchors.centerIn: parent
                font.pointSize: 20
            }

            opacity: mainWindow.isLoading ? 1 : 0

            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

        Keys.onPressed: {
            if ((event.modifiers & Qt.ControlModifier) !== 0) {
                if (event.key === Qt.Key_Q) {
                    mainWindow.close()
                }
            }
        }
    }
}
