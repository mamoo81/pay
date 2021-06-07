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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0
import Flowee.org.pay 1.0

import "./ControlColors.js" as ControlColors

ApplicationWindow {
    id: mainWindow
    visible: true
    width: Flowee.windowWidth === -1 ? 750 : Flowee.windowWidth
    height: Flowee.windowHeight === -1 ? 500 : Flowee.windowHeight
    minimumWidth: 800
    minimumHeight: 600
    title: "Flowee Pay"

    onWidthChanged: Flowee.windowWidth = width
    onHeightChanged: Flowee.windowHeight = height
    onVisibleChanged: if (visible) ControlColors.applySkin(mainWindow)

    property bool isLoading: typeof portfolio === "undefined";

    onIsLoadingChanged: {
        if (!isLoading) {
            if (!portfolio.current.isUserOwned) { // Open on receive tab if the wallet is effectively empty
                tabbar.currentIndex = 2;
            }
            else {
                tabbar.currentIndex = 0;
            }

            // delay loading to avoid errors due to not having a portfolio
            // portfolio is only initialized after a second or so.
            receivePane.source = "./ReceiveTransactionPane.qml"
            sendTransactionPane.source = "./SendTransactionPane.qml"
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
                anchors.fill: parent

                Pane {
                    property string title: qsTr("Activity")
                    property string icon: Flowee.useDarkSkin ? "activityIcon-light.png" : "activityIcon.png"
                    anchors.fill: parent
                    ListView {
                        id: activityView
                        model: isLoading || portfolio.current === null ? 0 : portfolio.current.transactions
                        clip: true
                        delegate: WalletTransaction { width: activityView.width }
                        anchors.fill: parent
                    }
                }
                Loader {
                    id: sendTransactionPane
                    anchors.fill: parent
                    property string title: qsTr("Send")
                    property string icon: Flowee.useDarkSkin ? "sendIcon-light.png" : "sendIcon.png"
                }
                Loader {
                    id: receivePane
                    anchors.fill: parent
                    property string icon: "receiveIcon.png"
                    property string title: qsTr("Receive")
                }
                Pane {
                    id: settingsPane
                    anchors.fill: parent
                    property string title: qsTr("Settings")
                    property string icon: Flowee.useDarkSkin ? "settingsIcon-light.png" : "settingsIcon.png"

                    GridLayout {
                        columns: 3

                        Label {
                            text: qsTr("Night mode") + ":"
                        }
                        FloweeCheckBox {
                            Layout.columnSpan: 2
                            checked: Flowee.useDarkSkin
                            onClicked: {
                                Flowee.useDarkSkin = !Flowee.useDarkSkin
                                ControlColors.applySkin(mainWindow);
                            }
                        }

                        Label {
                            text: qsTr("Unit") + ":"
                        }
                        ComboBox {
                            width: 210
                            model: {
                                var answer = [];
                                for (let i = 0; i < 5; ++i) {
                                    answer[i] = Flowee.nameOfUnit(i);
                                }
                                return answer;
                            }
                            currentIndex: Flowee.unit
                            onCurrentIndexChanged: {
                                Flowee.unit = currentIndex
                            }
                        }

                        Rectangle {
                            color: "#00000000"
                            radius: 6
                            border.color: mainWindow.palette.button
                            border.width: 2

                            implicitHeight: units.height + 10
                            implicitWidth: units.width + 10

                            GridLayout {
                                id: units
                                columns: 3
                                x: 5; y: 5
                                rowSpacing: 0
                                Label {
                                    text: {
                                        var answer = "1";
                                        for (let i = Flowee.unitAllowedDecimals; i < 8; ++i) {
                                            answer += "0";
                                        }
                                        return answer + " " + Flowee.unitName;
                                    }
                                    Layout.alignment: Qt.AlignRight
                                }
                                Label { text: "=" }
                                Label { text: "1 Bitcoin Cash" }

                                Label { text: "1 " + Flowee.unitName; Layout.alignment: Qt.AlignRight; visible: Flowee.isMainChain}
                                Label { text: "="; visible: Flowee.isMainChain}
                                Label {
                                    text: {
                                        var amount = 1;
                                        for (let i = 0; i < Flowee.unitAllowedDecimals; ++i) {
                                            amount = amount * 10;
                                        }
                                        return Fiat.formattedPrice(amount, Fiat.price);
                                    }
                                    visible: Flowee.isMainChain
                                }
                            }
                        }
                    }

                    Button {
                        anchors.right: parent.right
                        text: qsTr("Network Status")
                        onClicked: {
                            netView.source = "./NetView.qml"
                            netView.item.show();
                        }
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

                Item {
                    height: balanceLabel.height
                    width: parent.width
                    Label {
                        id: balanceLabel
                        text: qsTr("Balance");
                        height: implicitHeight / 10 * 7
                    }
                    Image {
                        anchors.right: parent.right
                        source: {
                            if (Flowee.hideBalance)
                                return Flowee.useDarkSkin ? "eye-closed-light.png" : "eye-closed.png"
                            return Flowee.useDarkSkin ? "eye-open-light.png" : "eye-open.png"
                        }
                        smooth: true
                        opacity: 0.5
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Flowee.hideBalance = !Flowee.hideBalance
                        }
                    }
                }

                Item {
                    height: balance.height
                    width: balance.width
                    BitcoinAmountLabel {
                        id: balance
                        opacity: blurredBalance.visible ? 0 : 1
                        value: {
                            if (isLoading)
                                return 0;
                            var account = portfolio.current;
                            if (account === null)
                                return 0;
                            if (Flowee.hideBalance)
                                return 88888888;
                            return account.balanceConfirmed + account.balanceUnconfirmed
                        }
                        colorize: false
                        showFiat: false
                        textColor: mainWindow.palette.text
                        fontPtSize: {
                            if (leftColumn.width < 300)
                                return mainWindow.font.pointSize * 2
                            return mainWindow.font.pointSize * 3
                        }
                    }
                    FastBlur {
                        id: blurredBalance
                        anchors.fill: parent
                        anchors.margins: -5
                        visible: Flowee.hideBalance
                        source: balance
                        radius: 58
                    }
                }
                Label {
                    text: {
                        if (Flowee.hideBalance)
                            return "-- " + Fiat.currencyName
                        return Fiat.formattedPrice(balance.value, Fiat.price)
                    }
                    opacity: 0.6
                }
                Item { // spacer
                    width: 10
                    height: 20
                }
                Label {
                    id: totalBalanceLabel
                    visible: mainWindow.isLoading || portfolio.accounts.length > 1
                    text: qsTr("Total balance");
                    height: implicitHeight / 10 * 9
                }
                Item {
                    visible: totalBalanceLabel.visible
                    width: totalBalance.width
                    height: totalBalance.height
                    BitcoinAmountLabel {
                        id: totalBalance
                        value: {
                            if (isLoading)
                                return 0;
                            if (Flowee.hideBalance)
                                return 88888888;
                            return portfolio.totalBalance
                        }
                        colorize: false
                        showFiat: false
                        fontPtSize: mainWindow.font.pointSize * 2
                        opacity: blurredTotalBalance.visible ? 0 : 1
                    }
                    FastBlur {
                        id: blurredTotalBalance
                        anchors.fill: parent
                        anchors.margins: -5
                        visible: Flowee.hideBalance
                        source: totalBalance
                        radius: 58
                    }
                }
                Label {
                    text: {
                        if (Flowee.hideBalance)
                            return "-- " + Fiat.currencyName
                        return Fiat.formattedPrice(totalBalance.value, Fiat.price)
                    }
                    visible: totalBalanceLabel.visible
                    opacity: 0.6
                }
                Item { // spacer
                    visible: fiatValue.visible
                    width: 10
                    height: 20
                }
                Label {
                    id: fiatValue
                    text: qsTr("1 BCH is: %1").arg(Fiat.formattedPrice(100000000, Fiat.price))
                    visible: Flowee.isMainChain
                    font.pixelSize: 18
                }
                Item { // spacer
                    visible: totalBalanceLabel.visible
                    width: 10
                    height: 40
                }
                Label {
                    text: qsTr("Network status")
                    opacity: 0.6
                }
                SyncIndicator {
                    accountBlockHeight: isLoading || portfolio.current === null ? 0 : portfolio.current.lastBlockSynched
                    visible: !isLoading && portfolio.current !== null
                }
                Item { // spacer
                    width: 10
                    height: 60
                }
                Repeater { // the portfolio listing our accounts
                    width: parent.width
                    model: (typeof portfolio === "undefined") ? 0 : portfolio.accounts;
                    delegate: AccountListItem {
                        width: leftColumn.width
                        account: modelData
                    }
                }
                Item { // spacer
                    width: 10
                    height: 40
                }

                Rectangle {
                    color: mainWindow.floweeGreen
                    radius: 10
                    width: leftColumn.width
                    height: buttonLabel.height + 30
                    Text {
                        id: buttonLabel
                        anchors.centerIn: parent
                        width: parent.width - 20
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        text: qsTr("Import your Bitcoin Cash wallet")
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: newAccountDiag.source = "./NewAccountDialog.qml"
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
                text: qsTr("Preparing...")
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

        Loader {
            id: accountDetailsDialog
            property QtObject account: null
            function open(account_) {
                account = account_;
                if (item) {
                    item.account = accountDetailsDialog.account;
                    item.raise();
                } else {
                    source = "./AccountDetails.qml";
                }
            }

            onLoaded: {
                item.account = accountDetailsDialog.account
                ControlColors.applySkin(item)
                accountDetailsHandler.target = item
            }
            Connections {
                id: accountDetailsHandler
                function onVisibleChanged() {
                    if (!accountDetailsDialog.item.visible) {
                        accountDetailsDialog.source = ""
                        accountDetailsDialog.account = null
                    }
                }
            }
        }
        // NetView (reachable from settings)
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
    }
}
