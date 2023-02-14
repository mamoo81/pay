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
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;

ListView {
    id: root

    property string icon: "qrc:/homeButtonIcon" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
    property string  title: qsTr("Home")

    Rectangle {
        id: backToTopButton
        width: 60
        height: 60
        anchors.right: parent.right
        y: {
            var indexAtTopOfScreen = root.indexAt(10, root.contentY + 10);
            if (indexAtTopOfScreen > 3)
                return 0;
            return height * -1; // out of screen.
        }
        color: "#66000000"

        Image {
            id: upIcon
            source: "qrc:/back-arrow.svg"
            rotation: 90
            width: 15
            height: 20
            anchors.centerIn: parent
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.positionViewAtIndex(0, ListView.Contain);
        }

        Behavior on y {
            NumberAnimation {
                easing.type: Easing.OutElastic
                duration: 500
            }
        }
    }

    /*
      The structure here is a bit funny.
      But we want to have the entire page scroll in order to show all the transactions we can.
      To avoid a mess of two scroll-areas (of Flickables) we simply make the top part into a
      header of the listview.
     */
    header: Rectangle {
        color: root.palette.window
        width: root.width
        height: column.height
        Column {
            id: column
            width: root.width - 20
            x: 10
            y: 10

            Flowee.BitcoinAmountLabel {
                opacity: Pay.hideBalance ? 0.2 : 1
                fontPixelSize: 34
                value: {
                    if (Pay.hideBalance)
                        return 88888888;
                    return portfolio.current.balanceConfirmed + portfolio.current.balanceUnconfirmed
                }
                colorize: false
            }

            AccountSyncState {
                account: portfolio.current
                width: parent.width
            }

            Row {
                width: parent.width
                height: dummyButton.height
                Flowee.ImageButton {
                    source: "qrc:/qr-code" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
                    width: parent.width / 3
                    onClicked: thePile.push("PayWithQR.qml")
                    iconSize: 40
                    height: dummyButton.height
                }
                IconButton {
                    id: dummyButton
                    width: parent.width / 3
                    text: qsTr("Scheduled")
                }
                Flowee.ImageButton {
                    width: parent.width / 3
                    height: dummyButton.height
                    iconSize: 50
                    source: "qrc:/receive.svg"
                    onClicked: switchToTab(2) // receive tab
                }
            }

            /* TODO
              "Is archive" / "Unrchive""

              Is Encryopted / Decrypt
             */

            Item { width: 10; height: 15 } // spacer
        }
    }

    model: portfolio.current.transactions
    clip: true
    width: parent.width
    height: contentHeight
    focus: true
    reuseItems: true
    section.property: "grouping"
    section.delegate: Item {
        height: label.height + 15
        width: root.width
        Rectangle {
            color: root.palette.light
            anchors.fill: parent
        }
        Flowee.Label {
            id: label
            x: 10
            y: 12
            font.bold: true
            font.pixelSize: mainWindow.font.pixelSize * 1.1
            text: portfolio.current.transactions.groupingPeriod(section);
        }
    }
    delegate: Item {
        id: transactionDelegate
        property var placementInGroup: model.placementInGroup

        width: root.width
        height: 80
        clip: true

        Rectangle {
            width: parent.width - 16
            x: 8
            visible: transactionDelegate.placementInGroup !== Wallet.Ungrouped;
            // we always have the rounded circles, but if we should not see them, we move them out of the clipped area.
            height: {
                var h = 80
                if (transactionDelegate.placementInGroup !== Wallet.GroupStart)
                    h += 20;
                if (transactionDelegate.placementInGroup !== Wallet.GroupEnd)
                    h += 20;
                return h;
            }
            y: transactionDelegate.placementInGroup === Wallet.GroupStart ? 0 : -20;

            radius: 20
            color: root.palette.alternateBase
            border.width: 1
            border.color: root.palette.midlight
        }

        Item {
            id: ruler
            width: parent.width
            height: 6
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            width: 45
            height: 45
            radius: 22.5
            x: 20
            anchors.verticalCenter: ruler.verticalCenter
        }

        Flowee.Label {
            id: commentLabel
            anchors.bottom: ruler.top
            anchors.right: price.left
            anchors.left: parent.left
            anchors.leftMargin: 80
            clip: true // TODO wordwrap?
            text: {
                var comment = model.comment
                if (comment !== "")
                    return comment;

                if (model.isCoinbase)
                    return qsTr("Miner Reward");
                if (model.isCashFusion)
                    return qsTr("Cash Fusion");
                if (model.fundsIn === 0)
                    return qsTr("Received");
                let diff = model.fundsOut - model.fundsIn;
                if (diff < 0 && diff > -1000) // then the diff is likely just fees.
                    return qsTr("Moved");
                return qsTr("Sent");
            }
        }
        QQC2.Label {
            anchors.top: ruler.bottom
            anchors.left: commentLabel.left
            color: palette.text
            text: {
                var minedHeight = model.height;
                if (minedHeight === -1) { // unconfirmed.
                    // We show a more 'in-progress' like string that indicates its not date-stamped yet.
                    if (model.fundsIn > 0)
                        return qsTr("Sending");
                    return qsTr("Seen");
                }
                if (minedHeight === -2)
                    return qsTr("Rejected")
                var date = model.date;
                var today = new Date();
                if (date.getFullYear() === today.getFullYear() && date.getMonth() === today.getMonth()) {
                    let day = today.getDate();
                    if (date.getDate() === day || date.getDate() === day - 1) {
                        // Then this is an item in the 'today' or the 'yesterday' group.
                        // specify more specific date/time
                        return Qt.formatTime(date);
                    }
                }

                return Pay.formatDate(model.date);
            }
        }

        Rectangle {
            id: price
            width: amount.width + 10
            height: amount.height + 10
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: ruler.verticalCenter
            radius: 6

            property int amountBch: model.fundsOut - model.fundsIn
            color: amountBch < 0 ? "#00000000"
                     : (Pay.useDarkSkin ? "#1d6828" : "#97e282") // green background
            Flowee.Label {
                id: amount
                text: {
                    var dat = model.date;
                    if (typeof dat === "undefined") // unconfirmed transactions have no date
                        var fiatPrice = Fiat.price;
                    else
                        fiatPrice = Fiat.historicalPrice(dat);
                    return Fiat.formattedPrice(price.amountBch, fiatPrice);
                }
                anchors.centerIn: parent
                opacity: Math.abs(price.amountBch) < 2000 ? 0.5 : 1
            }
        }

        Rectangle {
            visible: transactionDelegate.placementInGroup !== Wallet.GroupEnd
                         && transactionDelegate.placementInGroup !== Wallet.Ungrouped;
            anchors.bottom: parent.bottom
            height: 1
            width: parent.width - 16
            x: 8
            color: root.palette.midlight
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.currentIndex = index;
                var newItem = popupOverlay.open(selectedItem, transactionDelegate);
                newItem.infoObject = portfolio.current.txInfo(model.walletIndex, parent);
                root.model.freezeModel = true;
            }
            Connections {
                target: popupOverlay
                // unfreeze the model on closing of the popup
                function onIsOpenChanged() {
                    if (!popupOverlay.isOpen)
                        root.model.freezeModel = false;
                }
            }
        }
        Component {
            id: selectedItem
            TxInfoSmall { }
        }
    }
    displaced: Transition { NumberAnimation { properties: "y"; duration: 400 } }

    Keys.forwardTo: Flowee.ListViewKeyHandler {
        target: root
    }
}
