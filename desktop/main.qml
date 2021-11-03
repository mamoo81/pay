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
import "widgets" as Flowee
import "./ControlColors.js" as ControlColors

ApplicationWindow {
    id: mainWindow
    visible: true
    width: Pay.windowWidth === -1 ? 750 : Pay.windowWidth
    height: Pay.windowHeight === -1 ? 500 : Pay.windowHeight
    minimumWidth: 800
    minimumHeight: 600
    title: "Flowee Pay"

    onWidthChanged: Pay.windowWidth = width
    onHeightChanged: Pay.windowHeight = height
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

    Item {
        id: mainScreen
        anchors.fill: parent
        focus: true

        Rectangle {
            id: header
            color: Pay.useDarkSkin ? "#00000000" : mainWindow.floweeBlue
            width: parent.width
            height: {
                var h = mainWindow.height;
                if (h > 800)
                    return 120;
                return h / 800 * 120;
            }

            Rectangle {
                color: mainWindow.floweeBlue
                opacity: Pay.useDarkSkin ? 1 : 0

                width: parent.height / 5 * 4
                height: width
                radius: width / 2
                x: 2
                y: 8
                Behavior on opacity { NumberAnimation { duration: 300 } }
            }

            Image {
                id: appLogo
                anchors.verticalCenter: parent.verticalCenter
                x: 20
                smooth: true
                source: "FloweePay-light.svg"
                // ratio: 77 / 449
                height: (parent.height - 20) * 7 / 10
                width: height * 449 / 77
            }

            Item {
                id: balanceInHeader
                visible: {
                    if (mainWindow.isLoading)
                        return false;
                    if (portfolio.accounts.length <= 1)
                        return false;
                    // If there is not enough space here (only for quite long balances), move to the left bar
                    var minX = appLogo.width + totalFiatLabel.width + 50 // 50 is spacing
                    return x > minX;
                }
                width: totalBalance2.width
                height: totalBalance2.height
                anchors.bottom: parent.bottom
                anchors.bottomMargin: tabbar.headerHeight + 5
                anchors.right: parent.right
                anchors.rightMargin: 10
                baselineOffset: totalBalance2.baselineOffset

                BitcoinAmountLabel {
                    id: totalBalance2
                    value: {
                        if (isLoading)
                            return 0;
                        if (Pay.hideBalance)
                            return 88888888;
                        return portfolio.totalBalance
                    }
                    colorize: false
                    color: "white"
                    showFiat: false
                    fontPtSize: mainWindow.font.pointSize * 2
                    opacity: blurredTotalBalance2.visible ? 0 : 1
                }
                FastBlur {
                    id: blurredTotalBalance2
                    anchors.fill: parent
                    anchors.margins: 5
                    visible: Pay.hideBalance
                    source: totalBalance2
                    radius: 58
                }
            }
            Label {
                id: totalFiatLabel
                anchors.baseline: balanceInHeader.baseline
                anchors.right: balanceInHeader.left
                anchors.rightMargin: 10
                color: "white"

                text: {
                    if (Pay.hideBalance && Pay.isMainChain)
                        return "-- " + Fiat.currencyName
                    return Fiat.formattedPrice(totalBalance.value, Fiat.price)
                }
                visible: balanceInHeader.visible
                opacity: 0.6
            }
        }

        Item {
            id: tabbedPane
            width: parent.width * 65 / 100
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.topMargin: -1 * tabbar.headerHeight
            anchors.bottom: parent.bottom

            Rectangle {
                anchors.fill: parent
                opacity: 0.2
                anchors.topMargin: -5
                anchors.leftMargin: -5
                radius: 5
                color: Pay.useDarkSkin ? "black" : "white"
            }
            Flowee.TabBar {
                id: tabbar
                anchors.fill: parent

                Pane {
                    property string title: qsTr("Activity")
                    property string icon: "qrc:/activityIcon-light.png"
                    anchors.fill: parent
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -10
                        color: mainWindow.palette.light
                        radius: 10
                    }
                    ListView {
                        id: activityView
                        model: isLoading || portfolio.current === null ? 0 : portfolio.current.transactions
                        clip: true
                        delegate: WalletTransaction { width: activityView.width }
                        anchors.fill: parent
                        ScrollBar.vertical: Flowee.ScrollThumb {
                            id: thumb
                            minimumSize: 20 / activityView.height
                            preview: Rectangle {
                                width: label.width + 20
                                height: label.height + 20
                                radius: 5
                                color: label.palette.dark
                                Label {
                                    id: label
                                    anchors.centerIn: parent
                                    color: palette.light
                                    text: isLoading ? "" : activityView.model.dateForItem(thumb.position);
                                }
                            }
                        }

                        onModelChanged: resetUnreadTimer.restart();
                        Timer {
                            id: resetUnreadTimer
                            interval: 46 * 1000
                            // remove the 'new transaction' indicator.
                            onTriggered: portfolio.current.transactions.lastSyncIndicator = undefined
                            running: activityView.enabled && activityView.visible

                            // copy the height so we know when the account height changes
                            // and we can restart our timer as a result
                            property int height: syncIndicator.accountBlockHeight
                            onHeightChanged: restart();
                        }
                    }
                }
                Loader {
                    id: sendTransactionPane
                    anchors.fill: parent
                    property string title: qsTr("Send")
                    property string icon: "qrc:/sendIcon-light.png"
                }
                Loader {
                    id: receivePane
                    anchors.fill: parent
                    property string icon: "qrc:/receiveIcon.png"
                    property string title: qsTr("Receive")
                }
                SettingsPane {
                    anchors.fill: parent
                }
            }
            Behavior on opacity { NumberAnimation { } }
            visible: opacity > 0
        }

        Loader {
            id: accountDetails
            anchors.bottom: parent.bottom
            anchors.left: overviewPane.right
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.topMargin: -1 * tabbar.headerHeight
            opacity: 0
            Behavior on opacity { NumberAnimation { } }

            states: [
                State {
                    name: "showTransactions"
                    PropertyChanges { target: tabbedPane; opacity: 1 }
                    PropertyChanges { target: accountDetails; opacity: 0 }
                    PropertyChanges { target: accountDetails; source: "" }
                },
                State {
                    name: "accountDetails"
                    PropertyChanges { target: accountDetails; source: "./AccountDetails.qml" }
                    PropertyChanges { target: accountDetails; opacity: 1 }
                    PropertyChanges { target: tabbedPane; opacity: 0 }
                }
            ]
            state: "showTransactions"
        }

        Flickable {
            id: overviewPane
            anchors.left: parent.left
            anchors.right: tabbedPane.left
            anchors.bottom: parent.bottom
            anchors.top: header.bottom
            contentWidth: leftColumn.width
            contentHeight: leftColumn.height
            flickableDirection: Flickable.VerticalFlick
            clip: true

            Column {
                id: leftColumn
                x: 10
                width: overviewPane.width - 60

                // the total balance is for most people not this one, but the balanceInHeader one.
                // we only show this one when the header one decides it can't be seen.
                Label {
                    id: totalBalanceLabel
                    visible: (mainWindow.isLoading || portfolio.accounts.length > 1) && !balanceInHeader.visible
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
                            if (Pay.hideBalance)
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
                        visible: Pay.hideBalance
                        source: totalBalance
                        radius: 58
                    }
                }
                Label {
                    text: {
                        if (Pay.hideBalance && Pay.isMainChain)
                            return "-- " + Fiat.currencyName
                        return Fiat.formattedPrice(totalBalance.value, Fiat.price)
                    }
                    visible: totalBalanceLabel.visible
                    opacity: 0.6
                }
                Item { // spacer
                    width: 10
                    height: 50
                }
                Item {
                    height: balanceLabel.height
                    width: parent.width
                    Label {
                        id: balanceLabel
                        text: qsTr("Balance");
                        height: implicitHeight / 10 * 7
                    }
                    Image {
                        id: showBalanceButton
                        anchors.right: parent.right
                        source: {
                            if (Pay.hideBalance)
                                return Pay.useDarkSkin ? "eye-closed-light.png" : "eye-closed.png"
                            return Pay.useDarkSkin ? "eye-open-light.png" : "eye-open.png"
                        }
                        smooth: true
                        opacity: 0.5
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                Pay.hideBalance = !Pay.hideBalance;
                                balanceDetailsPane.showDetails = false;
                            }
                        }
                    }

                    Label {
                        id: questionMark
                        text: "🛈"
                        ToolTip.text: qsTr("Show Wallet Details")
                        anchors.right: showBalanceButton.left
                        anchors.baseline: balanceLabel.baseline
                        anchors.rightMargin: 10
                        opacity: 0.5
                        visible: !mainWindow.isLoading && !portfolio.accounts[0].isUserOwned
                        MouseArea {
                            anchors.fill: parent
                            onClicked: accountDetails.state = "accountDetails";
                        }
                    }
                }

                Item {
                    id: balanceDetailsPane
                    property bool showDetails: false
                    width: balance.width
                    height: balance.height + (showDetails ? extraBalances.height + 20 : 0)
                    BitcoinAmountLabel {
                        id: balance
                        opacity: blurredBalance.visible ? 0 : 1
                        value: {
                            if (isLoading)
                                return 0;
                            var account = portfolio.current;
                            if (account === null)
                                return 0;
                            if (Pay.hideBalance)
                                return 88888888;
                            return account.balanceConfirmed + account.balanceUnconfirmed
                        }
                        colorize: false
                        showFiat: false
                        color: mainWindow.palette.text
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
                        visible: Pay.hideBalance
                        source: balance
                        radius: 58
                    }
                    GridLayout {
                        id: extraBalances
                        visible: parent.showDetails
                        width: parent.width - 30
                        anchors.top: balance.bottom
                        anchors.topMargin: 10
                        columns: 2
                        x: 25

                        property QtObject account: mainWindow.isLoading ? null : portfolio.current
                        Label {
                            text: qsTr("Main") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        BitcoinAmountLabel {
                            value: extraBalances.account == null ? 0 : extraBalances.account.balanceConfirmed
                            colorize: false
                            showFiat: false
                        }
                        Label {
                            text: qsTr("Unconfirmed") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        BitcoinAmountLabel {
                            value: extraBalances.account == null ? 0 : extraBalances.account.balanceUnconfirmed
                            colorize: false
                            showFiat: false
                        }
                        Label {
                            text: qsTr("Immature") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        BitcoinAmountLabel {
                            value: extraBalances.account == null ? 0 : extraBalances.account.balanceImmature
                            colorize: false
                            showFiat: false
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (!Pay.hideBalance)
                                parent.showDetails = !parent.showDetails
                        }
                    }

                    Behavior on height { NumberAnimation {} }
                }
                Label {
                    text: {
                        if (Pay.hideBalance && Pay.isMainChain)
                            return "-- " + Fiat.currencyName
                        return Fiat.formattedPrice(balance.value, Fiat.price)
                    }
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
                    visible: Pay.isMainChain
                    font.pixelSize: 18
                }
                Item { // spacer
                    width: 10
                    height: totalBalanceLabel.visible ? 40 : 10
                }
                Label {
                    text: qsTr("Network status")
                    opacity: 0.6
                }
                SyncIndicator {
                    id: syncIndicator
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
                        text: qsTr("Add Bitcoin Cash wallet")
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: newAccountPane.source = "./NewAccountPane.qml"
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
            color: mainWindow.palette.window
            anchors.fill: parent
            Label {
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
        // new accounts pane, corresponding to the big green button
        Loader {
            id: newAccountPane
            anchors.fill: parent
            onLoaded: {
                tabbar.enabled = false // avoid it taking focus on tab
                newAccountHandler.target = item // to unload on hide
            }
            Connections {
                id: newAccountHandler
                function onVisibleChanged() {
                    if (!newAccountPane.item.visible) {
                        newAccountPane.source = ""
                        tabbar.enabled = true // reenable
                        tabbar.focus = true;
                    }
                }
            }
        }
    }
}
