/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
import QtQuick.Controls
import QtQuick.Layouts
import "../Flowee" as Flowee
import "../ControlColors.js" as ControlColors

import QtQuick.Controls.Basic

ApplicationWindow {
    id: mainWindow
    visible: true
    width: Pay.windowWidth === -1 ? 750 : Pay.windowWidth
    height: Pay.windowHeight === -1 ? 500 : Pay.windowHeight
    minimumWidth: 400
    minimumHeight: 300
    title: "Flowee Pay"

    onWidthChanged: Pay.windowWidth = width
    onHeightChanged: Pay.windowHeight = height
    onVisibleChanged: if (visible) ControlColors.applySkin(mainWindow)

    property bool isLoading: typeof portfolio === "undefined";

    onIsLoadingChanged: {
        if (!isLoading) {
            // delay loading to avoid errors due to not having a portfolio
            // portfolio is only initialized after a second or so.
            receivePane.source = "./ReceiveTransactionPane.qml"
            sendTransactionPane.source = "./SendTransactionPane.qml"

            if (!portfolio.current.isUserOwned) { // Open on receive tab if the wallet is effectively empty
                tabbar.currentIndex = 2;
            }
            else if (Pay.paymentProtocolRequestChanged !== "") {
                tabbar.currentIndex = 1;
            }
            else {
                tabbar.currentIndex = 0;
            }
        }
    }

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"
    property color errorRed: Pay.useDarkSkin ? "#ff6568" : "#940000"
    property color errorRedBg: Pay.useDarkSkin ? "#671314" : "#9f1d1f"

    Item {
        id: mainScreen
        anchors.fill: parent
        focus: true

        Rectangle {
            id: header
            color: Pay.useDarkSkin ? "#00000000" : mainWindow.floweeBlue
            width: parent.width
            height: 90

            Rectangle {
                color: mainWindow.floweeBlue
                opacity: Pay.useDarkSkin ? 1 : 0

                width: 60
                height: 60
                radius: 30
                x: 3
                y: 11
                Behavior on opacity { NumberAnimation { duration: 300 } }
            }

            Image {
                id: appLogo
                anchors.verticalCenter: parent.verticalCenter
                x: 17
                smooth: true
                source: "qrc:/FloweePay-light.svg"
                // ratio: 77 / 449
                height: 40
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
                width: totalBalance.width
                height: totalBalance.height
                anchors.bottom: parent.bottom
                anchors.bottomMargin: tabbar.headerHeight + 5
                anchors.right: parent.right
                anchors.rightMargin: 10
                baselineOffset: totalBalance.baselineOffset

                Flowee.BitcoinAmountLabel {
                    id: totalBalance
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
                    fontPixelSize: 28
                    opacity: Pay.hideBalance ? 0.2 : 1
                }
            }
            Label {
                id: totalFiatLabel
                anchors.baseline: balanceInHeader.baseline
                anchors.right: balanceInHeader.left
                anchors.rightMargin: 10
                color: "white"
                font.pixelSize: 15

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
            width: Math.max(parent.width - 270, parent.width * 65 / 100)
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
                        color: palette.light
                        radius: 10
                    }
                    Column {
                        id: activityHeader
                        width: parent.width
                        spacing: 6
                        Rectangle {
                            width: parent.width
                            height: warn.height + unarchiveButton.height + 26
                            color: Pay.useDarkSkin ? "#c1ba58" : "#f6e992" // yellow
                            visible: !isLoading && portfolio.current.isArchived
                            radius: 7
                            Flowee.Label {
                                id: warn
                                y: 6
                                x: 6
                                width: parent.width - 10
                                horizontalAlignment: Text.AlignHCenter

                                color: "black"
                                font.bold: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Archived wallets do not check for activities. Balance may be out of date.")
                            }
                            Flowee.Button {
                                id: unarchiveButton
                                text: qsTr("Unarchive")
                                anchors.right: warn.right
                                anchors.top: warn.bottom
                                anchors.topMargin: 6

                                onClicked: portfolio.current.isArchived = false
                            }
                        }

                        Rectangle {
                            id: needsDecryptPane
                            width: parent.width
                            height: decryptText.height + decryptPwd.height + decryptButton.height + 36
                            color: Pay.useDarkSkin ? "#c1ba58" : "#f6e992" // yellow
                            visible: !isLoading && portfolio.current.needsPinToOpen
                                        && !portfolio.current.isDecrypted
                            radius: 7
                            onVisibleChanged: {
                                decryptError.visible = false
                                decryptPwd.text = ""
                            }

                            Text {
                                id: decryptText
                                y: 6
                                x: 6
                                width: parent.width - 20
                                horizontalAlignment: Text.AlignHCenter

                                color: "black"
                                font.bold: true
                                wrapMode: Text.WordWrap
                                text: qsTr("This wallet needs a password to open.")
                            }
                            Text {
                                id: decryptLabel
                                anchors.left: decryptText.left
                                anchors.verticalCenter: decryptPwd.verticalCenter
                                color: decryptText.color
                                text: qsTr("Password:")

                            }
                            Flowee.TextField {
                                id: decryptPwd
                                focus: needsDecryptPane.visible
                                anchors.top: decryptText.bottom
                                anchors.left: decryptLabel.right
                                anchors.right: parent.right
                                anchors.margins: 6
                                echoMode: TextInput.Password
                                onAccepted: decryptButton.clicked()
                            }
                            Text {
                                id: decryptError
                                anchors.left: decryptPwd.left
                                anchors.verticalCenter: decryptButton.verticalCenter
                                color: "#830000"
                                font.bold: true
                                text: qsTr("Invalid password")
                                visible: false
                            }
                            Flowee.Button {
                                id: decryptButton
                                text: qsTr("Open")
                                anchors.right: decryptText.right
                                anchors.top: decryptPwd.bottom
                                anchors.topMargin: 6
                                enabled: decryptPwd.text !== ""

                                onClicked: {
                                    portfolio.current.decrypt(decryptPwd.text)
                                    if (portfolio.current.isDecrypted) {
                                        decryptPwd.text = ""
                                        decryptError.visible = false
                                    } else {
                                        decryptPwd.selectAll();
                                        decryptError.visible = true
                                        decryptPwd.forceActiveFocus();
                                    }
                                }
                            }
                        }
                    }
                    ListView {
                        id: activityView
                        model: isLoading ? 0 :  portfolio.current.transactions
                        clip: true
                        delegate: WalletTransaction { width: activityView.width }
                        anchors.top: activityHeader.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        ScrollBar.vertical: Flowee.ScrollThumb {
                            id: thumb
                            minimumSize: 20 / activityView.height
                            visible: size < 0.9
                            preview: Rectangle {
                                width: label.width + 12
                                height: label.height + 12
                                radius: 5
                                color: palette.light
                                Label {
                                    id: label
                                    anchors.centerIn: parent
                                    color: palette.dark
                                    text: isLoading || activityView.model === null ? "" : activityView.model.dateForItem(thumb.position);
                                }
                            }
                        }

                        onModelChanged: resetUnreadTimer.restart();
                        Timer {
                            id: resetUnreadTimer
                            interval: 90 * 1000
                            // remove the 'new transaction' indicator.
                            onTriggered: portfolio.current.transactions.lastSyncIndicator = undefined
                            running: activityView.enabled && activityView.visible

                            // copy the height so we know when the account height changes
                            // and we can restart our timer as a result
                            property int height: {
                                if (isLoading) return 0;
                                return portfolio.current.lastBlockSynched
                            }
                            onHeightChanged: restart();
                        }
                    }

                    Keys.forwardTo: Flowee.ListViewKeyHandler {
                        target: activityView
                    }
                }
                Loader {
                    id: sendTransactionPane
                    // Disable these tabs for archived accounts
                    enabled: !mainWindow.isLoading && !portfolio.current.isArchived
                             && (!portfolio.current.needsPinToOpen || portfolio.current.isDecrypted)
                    anchors.fill: parent
                    property string title: qsTr("Send")
                    property string icon: "qrc:/sendIcon-light.png"
                }
                Loader {
                    id: receivePane
                    enabled: sendTransactionPane.enabled && (!portfolio.current.needSpinToOpen || portfolio.current.isDecrypted)
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
            // This overlays the tabbed pane
            id: accountOverlay
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
                    PropertyChanges { target: tabbar; focus: true }
                    PropertyChanges { target: accountOverlay;
                        opacity: 0;
                        source: "";
                    }
                },
                State {
                    name: "accountDetails"
                    PropertyChanges { target: accountOverlay;
                        source: "./AccountDetails.qml"
                        opacity: 1
                        focus: true
                    }
                    PropertyChanges { target: tabbedPane; opacity: 0 }
                },
                State {
                    name: "startWalletEncryption"
                    PropertyChanges { target: accountOverlay;
                        source: "./WalletEncryption.qml"
                        opacity: 1
                        focus: true
                    }
                    PropertyChanges { target: tabbedPane; opacity: 0 }
                }
            ]
            state: "showTransactions"
        }

        Item {
            // the whole area left of the tabbed panels.
            id: overviewPane
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: tabbedPane.left
            anchors.rightMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            anchors.top: header.bottom

            Column {
                id: balances
                spacing: 3
                width: parent.width

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
                            var state = Pay.hideBalance ? "closed" : "open";
                            var skin = Pay.useDarkSkin ? "-light" : ""
                            return "qrc:/eye-" + state + skin + ".png";
                        }
                        smooth: true
                        opacity: 0.5
                        height: 14
                        width: 14
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                Pay.hideBalance = !Pay.hideBalance;
                                balanceDetailsPane.showDetails = false;
                            }
                        }
                    }
                }

                Item {
                    id: balanceDetailsPane
                    property bool showDetails: false
                    width: parent.width
                    clip: true // avoid the balance overlapping the tabbar.
                    height: balance.height + (showDetails ? extraBalances.height + 10 : 0)
                    Flowee.BitcoinAmountLabel {
                        id: balance
                        opacity: Pay.hideBalance ? 0.2 : 1
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
                        color: palette.windowText
                        fontPixelSize: {
                            if (leftColumn.width < 240) // max width is 252
                                return leftColumn.width / 9
                            return 27;
                        }
                    }

                    GridLayout {
                        id: extraBalances
                        visible: parent.showDetails
                        width: parent.width / 0.9
                        anchors.top: balance.bottom
                        anchors.topMargin: 5
                        columns: 2
                        scale: 0.9
                        transformOrigin: Item.TopLeft
                        clip: true

                        property QtObject account: mainWindow.isLoading ? null : portfolio.current
                        Label {
                            text: qsTr("Main", "balance (money), non specified") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.BitcoinAmountLabel {
                            value: extraBalances.account == null ? 0 : extraBalances.account.balanceConfirmed
                            colorize: false
                            showFiat: false
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("Unconfirmed", "balance (money)") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.BitcoinAmountLabel {
                            value: extraBalances.account == null ? 0 : extraBalances.account.balanceUnconfirmed
                            colorize: false
                            showFiat: false
                        }
                        Label {
                            text: qsTr("Immature", "balance (money)") + ":"
                            Layout.alignment: Qt.AlignRight
                        }
                        Flowee.BitcoinAmountLabel {
                            value: extraBalances.account == null ? 0 : extraBalances.account.balanceImmature
                            colorize: false
                            showFiat: false
                        }
                    }
                    MouseArea {
                        enabled: priceCover.visible === false
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!Pay.hideBalance)
                                parent.showDetails = !parent.showDetails
                        }
                    }

                    Rectangle {
                        id: priceCover // this covers the prices while the wallet is encrypted.
                        color: palette.window
                        opacity: 0.8
                        anchors.fill: parent
                        anchors.topMargin: 2
                        visible: !mainWindow.isLoading && portfolio.current.needsPinToOpen && !portfolio.current.isDecrypted
                    }

                    Behavior on height { NumberAnimation {} }
                }
                Label {
                    text: {
                        if (mainWindow.isLoading || !Pay.isMainChain)
                            return "";
                        if (Pay.hideBalance || (portfolio.current.needsPinToOpen && !portfolio.current.isDecrypted))
                            return "-- " + Fiat.currencyName;
                        return Fiat.formattedPrice(balance.value, Fiat.price);
                    }
                    opacity: 0.6
                }
                Item { width: 1; height: fiatValue.visible ? 10 : 0 } // spacer
                Item {
                    width: parent.width
                    height: fiatValue.height
                    Label {
                        id: fiatValue
                        property double prevPrice: 0
                        text: qsTr("1 BCH is: %1").arg(Fiat.formattedPrice(100000000, Fiat.price))
                        visible: Pay.isMainChain

                        Behavior on color { ColorAnimation { duration: 300 } }
                        onTextChanged: {
                            animTimer.start()
                            if (prevPrice > Fiat.price)
                                color = Pay.useDarkSkin ? "#5f1414" : "#ff3636"; // red
                            else
                                color = Pay.useDarkSkin ? "#154822" : "#4aff77"; // green
                            prevPrice = Fiat.price
                        }
                        Timer {
                            id: animTimer
                            interval: 305
                            onTriggered: fiatValue.color = palette.windowText
                        }
                    }

                    AccountConfigMenu {
                        anchors.right: parent.right
                        visible: isLoading ? false : portfolio.singleAccountSetup
                        account: isLoading ? null : portfolio.current
                    }
                }
            }

            Flickable {
                anchors {
                    left: balances.left
                    right: balances.right
                    top: balances.bottom
                    topMargin: 8
                    bottom: parent.bottom
                    bottomMargin: 8
                }
                contentWidth: leftColumn.width
                contentHeight: leftColumn.height
                flickableDirection: Flickable.VerticalFlick
                clip: true

                Column {
                    id: leftColumn
                    width: balances.width

                    Label {
                        text: qsTr("Network status")
                        opacity: 0.6
                    }
                    Label {
                        id: syncIndicator
                        text: {
                            if (isLoading)
                                return "";
                            var account = portfolio.current;
                            if (account === null)
                                return "";
                            if (account.needsPinToOpen && !account.isDecrypted)
                                return qsTr("Offline");
                            return account.timeBehind;
                        }
                        font.italic: true
                    }
                    Item { // spacer
                        width: 1
                        height: 20
                    }
                    Repeater { // the portfolio listing our accounts
                        width: parent.width
                        model: mainWindow.isLoading ? 0 : portfolio.accounts;
                        delegate: AccountListItem {
                            width: leftColumn.width
                            account: modelData
                        }
                    }
                    Item { // spacer
                        width: 1
                        height: 20
                    }

                    Rectangle { // button 'add bitcoin cash wallet'
                        color: mainWindow.floweeGreen
                        radius: 7
                        width: leftColumn.width
                        height: buttonLabel.height + 28
                        Text {
                            id: buttonLabel
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 10, implicitWidth)
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            text: qsTr("Add Bitcoin Cash wallet")
                            horizontalAlignment: Qt.AlignHCenter
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: newAccountPane.source = "./NewAccountPane.qml"
                        }
                    }
                    Item { // spacer
                        width: 1
                        height: 20
                    }
                    Item {
                        // archived wallets label with hide button
                        visible: !isLoading && portfolio.archivedAccounts.length > 0
                        height: archivedLabel.height
                        width: leftColumn.width
                        Flowee.ArrowPoint {
                            id: showArchivedWalletsList
                            property bool on: false
                            color: Pay.useDarkSkin ? "white" : "black"
                            rotation: on ? 90 : 0
                            transformOrigin: Item.Center
                            Behavior on rotation { NumberAnimation {} }
                        }
                        Label {
                            id: archivedLabel
                            x: showArchivedWalletsList.width + 6
                            text: {
                                if (isLoading)
                                    return ""
                                var walletCount = portfolio.archivedAccounts.length

                                return qsTr("Archived wallets [%1]", "Arg is wallet count", walletCount).arg(walletCount);
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: showArchivedWalletsList.on = !showArchivedWalletsList.on
                        }
                    }

                    Repeater { // the archived accounts
                        width: parent.width
                        model: showArchivedWalletsList.on ? portfolio.archivedAccounts : 0;
                        delegate: AccountListItem {
                            width: leftColumn.width
                            account: modelData
                            // archived accounts don't have access to anything but the activity tab
                            onClicked: tabbar.currentIndex = 0; // change to the 'activity' tab
                        }
                    }
                }
            }
        }

        Rectangle {
            id: splashScreen
            color: palette.window
            anchors.fill: parent
            Label {
                text: qsTr("Preparing...")
                anchors.centerIn: parent
                font.pointSize: 20
            }

            opacity: mainWindow.isLoading ? 1 : 0

            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

        Keys.onPressed: (event)=> {
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
