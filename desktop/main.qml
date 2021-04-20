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
// import QtQuick.Layouts 1.14
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

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"

    header: Rectangle {
        color: mainWindow.floweeBlue
        width: parent.width
        height: {
            var h = mainWindow.height;
            if (h > 700)
                return 120;
            return h / 700 * 120;
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
        Label {
            y: parent.height / 3 * 2 - height
            anchors.right: parent.right
            anchors.rightMargin: 30
            text: "BCH: " + "€1000"
            visible: Flowee.isMainChain
            font.pixelSize: 18
        }
    }

    Pane {
        id: mainScreen
        anchors.fill: parent

        Item {
            id: tabbedPane
            height: parent.height
            width: parent.width * 65 / 100
            anchors.right: parent.right

            Rectangle {
                anchors.fill: parent
                opacity: 0.2
                color: "black"
            }
            Row {
                width: parent.width
                height: activityButton.height
                PayTabButton {
                    id: activityButton
                    text: qsTr("Activity")
                    width: parent.width / 4
                }
                PayTabButton {
                    text: qsTr("Send")
                    width: parent.width / 4
                }
                PayTabButton {
                    text: qsTr("Receive")
                    width: parent.width / 4
                    isActive: true
                }
                PayTabButton {
                    text: qsTr("Settings")
                    width: parent.width / 4
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
                    id: totalBalenceLabel
                    visible: mainWindow.isLoading || portfolio.accounts.Length > 1
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
                    visible: totalBalenceLabel.visible
                    colorize: false
                    fontPtSize: mainWindow.font.pointSize * 2
                }
                Label {
                    text: qsTr("0.00 EUR"); // TODO
                    visible: totalBalenceLabel.visible && Flowee.isMainChain
                    opacity: 0.3
                }
                Item { // spacer
                    width: 10
                    height: 40
                }
                Label {
                    text: qsTr("Network status")
                    opacity: 0.3
                }
                Label {
                    text: qsTr("Synchronizing")
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
                                    return Flowee.useDarkSkin ? mainWindow.floweeGreen : mainWindow.floweeBlue;
                                if (hover)
                                    return mainWindow.floweeSalmon;
                                return Flowee.useDarkSkin ? "#EEE" : "#111"
                            }

                            Text {
                                id: name
                                font.bold: true
                                x: 10
                                y: 10
                                text: modelData.name
                                color: parent.border.color
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
    }

/*

    Image {
        anchors.centerIn: parent
        source: Flowee.useDarkSkin ? "FloweePay-light.svg" : "FloweePay.svg"
        width: parent.width / 10 * 9
        height: width * 77 / 449
    }
*/

/*
    menuBar: MenuBar {
        Menu {
            title: qsTr("Flowee &Pay")
            Action {
                text: qsTr("Create / Import...")
                onTriggered: newAccountDiag.source = "./NewAccountDialog.qml"
            }

            Action {
                text: qsTr("&Quit")
                shortcut: StandardKey.Quit
                onTriggered: mainWindow.close()
            }
        }
        Menu {
            title: qsTr("&View")

            Action {
                text: qsTr("&Account List")
                onTriggered: portfolio.current = null
            }

            Action {
                checkable: true
                checked: Flowee.useDarkSkin
                text: qsTr("&Dark Mode")
                onTriggered: {
                    Flowee.useDarkSkin = checked
                    ControlColors.applySkin(mainWindow)
                }
            }
            Action {
                enabled: !mainWindow.isLoading
                text: qsTr("Network Status")
                onTriggered: {
                    netView.source = "NetView.qml"
                    netView.item.show();
                }
            }
            MenuSeparator { }
            Menu {
                title: qsTr("Unit Indicator")
                Action {
                    text: "BCH"
                    checkable: true
                    checked: Flowee.unit === Pay.BCH
                    onTriggered: Flowee.unit = Pay.BCH
                }
                Action {
                    text: "mBCH"
                    checkable: true
                    checked: Flowee.unit === Pay.MilliBCH
                    onTriggered: Flowee.unit = Pay.MilliBCH
                }
                Action {
                    text: "Bits"
                    checkable: true
                    checked: Flowee.unit === Pay.Bits
                    onTriggered: Flowee.unit = Pay.Bits
                }
                Action {
                    text: "µBCH"
                    checkable: true
                    checked: Flowee.unit === Pay.MicroBCH
                    onTriggered: Flowee.unit = Pay.MicroBCH
                }
                Action {
                    text: "Satoshis"
                    checkable: true
                    checked: Flowee.unit === Pay.Satoshis
                    onTriggered: Flowee.unit = Pay.Satoshis
                }
            }
        }
    }
*/

/*
    AccountSelectionPage {
        id: accountSelectionPage
        anchors.fill: parent
        visible: !isLoading && portfolio.current === null
    }
*/

/*
    AccountPage {
        anchors.fill: parent
    }
*/

    // NetView (reachable from menu)

/*
    Loader {
        id: netView
        onLoaded: {
            ControlColors.applySkin(item)
            netViewHandler.target = item
        }
        Connections {
            id: netViewHandler
            function onVisibleChanged() {
                if (!netView.item.visible)
                    netView.source = ""
            }
        }
    }
    Loader {
        id: newAccountDiag
        onLoaded: {
            ControlColors.applySkin(item)
            newAccountHandler.target = item
        }
        Connections {
            id: newAccountHandler
            function onVisibleChanged() {
                if (!newAccountDiag.item.visible) {
                    newAccountDiag.source = ""
                }
            }
        }
    }
    Loader {
        id: accountDetailsDialog
        onLoaded: {
            item.account = portfolio.current
            ControlColors.applySkin(item)
            accountDetailsHandler.target = item
        }
        Connections {
            id: accountDetailsHandler
            function onVisibleChanged() {
                if (!accountDetailsDialog.item.visible) {
                    accountDetailsDialog.source = ""
                }
            }
        }
    }
*/

/*
    footer: Pane {
        contentWidth: parent.width
        contentHeight: statusBar.height
        Label {
            id: statusBar
            property string message: ""
            text: {
                if (mainWindow.isLoading)
                    return qsTr("Starting up...");
                if (portfolio.current === null)
                    return qsTr("%1 Accounts", "", portfolio.accounts.length).arg(portfolio.accounts.length);
                if (message === "")
                    return portfolio.current.name
                return message;
            }
        }
    }
*/
}
