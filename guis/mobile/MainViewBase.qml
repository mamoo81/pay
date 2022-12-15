/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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

QQC2.Control {
    id: root
    width: parent.width
    height: parent.height

    // This trick  means any child items are actually added to the 'stack' item's children.
    default property alias content: stack.children
    property int currentIndex: 0
    onCurrentIndexChanged: setOpacities()
    Component.onCompleted: setOpacities()

    function setOpacities() {
        for (let i = 0; i < stack.children.length; ++i) {
            let on = i === currentIndex;
            let child = stack.children[i];
            child.visible = on;
        }
        takeFocus();
    }
    function switchToTab(index) {
        currentIndex = index
    }

    // called from main when this page becomes active, as well as when we change tabs
    function takeFocus() {
        // try to make sure the current tab gets keyboard focus properly.
        // We do some extra work in case the tab itself is a loader.
        forceActiveFocus();
        let visibleChild = stack.children[currentIndex];
        visibleChild.focus = true
        if (visibleChild instanceof Loader) {
            var child = visibleChild.item;
            if (child !== null)
                child.focus = true;
        }
    }
    Rectangle {
        id: header
        width: parent.width
        height: 50
        color: Pay.useDarkSkin ? root.palette.base : mainWindow.floweeBlue

        Column {
            id: menuButton
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter
            x: 8

            Repeater {
                model: 3
                delegate: Rectangle {
                    color: "white"
                    width: 12
                    height: 3
                    radius: 2
                }
            }
        }
        MouseArea {
            anchors.fill: menuButton
            anchors.margins: -20
            cursorShape: Qt.PointingHandCursor
            onClicked: menuOverlay.open = true;
        }

        QQC2.Control {
            id: logo
            // Here we just want the text part. So clip that out.
            clip: true
            y: 17
            x: 20
            width: 122
            height: 21
            baselineOffset: 16
            Image {
                source: "qrc:/FloweePay.svg"
                // ratio: 449 / 77
                width: 150
                height: 26
                x: -28
                y: -5
            }
        }

        QQC2.Label {
            id: currentWalletName
            visible: portfolio.current.isUserOwned || portfolio.accounts.length >= 1
            text:  portfolio.current.name
            color: "#fcfcfc"
            clip: true
            anchors.left: logo.right
            anchors.right: searchIcon.left
            anchors.margins: 12
            anchors.baseline: logo.baseline

            MouseArea {
                anchors.fill: parent
                onClicked: accountSelector.open();
            }
        }

        QQC2.Label {
            id: searchIcon
            color: "#fcfcfc"
            text: "" // placeholder for the magnifying glass search feature to be done in future
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.baseline: logo.baseline
        }

        QQC2.Popup {
            id: accountSelector
            closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent
            y: header.height
            width: root.width
            height: columnLayout.height

            Connections {
                target: menuOverlay
                function onOpenChanged() { accountSelector.close(); }
            }

            ColumnLayout {
                id: columnLayout
                width: parent.width

                Flowee.Label {
                    text: qsTr("Your Wallets")
                    font.bold: true
                }

                Repeater { // portfolio holds all our accounts
                    width: parent.width
                    model: portfolio.accounts
                    delegate: Item {
                        width: columnLayout.width
                        height: accountName.height + lastActive.height + 6 * 3
                        Rectangle {
                            color: root.palette.button
                            radius: 5
                            anchors.fill: parent
                            visible: modelData === portfolio.current
                        }
                        Flowee.Label {
                            id: accountName
                            y: 6
                            x: 6
                            text: modelData.name
                        }
                        Flowee.Label {
                            id: fiat
                            anchors.top: accountName.top
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            text: Fiat.formattedPrice(modelData.balanceConfirmed + modelData.balanceUnconfirmed, Fiat.price)
                        }
                        Flowee.Label {
                            id: lastActive
                            anchors.top: accountName.bottom
                            anchors.left: accountName.left
                            text: qsTr("last active") + ": " + Pay.formatDate(modelData.lastMinedTransaction)
                            font.pixelSize: root.font.pixelSize * 0.8
                            font.bold: false
                        }
                        Flowee.BitcoinAmountLabel {
                            anchors.right: fiat.right
                            anchors.top: lastActive.top
                            font.pixelSize: lastActive.font.pixelSize
                            showFiat: false
                            value: modelData.balanceConfirmed + modelData.balanceUnconfirmed
                            colorize: false
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                portfolio.current = modelData
                                accountSelector.close();
                            }
                        }
                    }
                }
                Item {
                    // horizontal divider.
                    width: parent.width
                    height: 1
                    Rectangle {
                        height: 1
                        width: parent.width / 10 * 7
                        x: (parent.width - width) / 2 // center in column
                        color: root.palette.highlight
                    }
                }
            }
        }
    }

    Item {
        id: stack
        width: root.width
        anchors.top: header.bottom; anchors.bottom: tabbar.top
    }
    Row {
        id: tabbar
        anchors.bottom: parent.bottom

        Repeater {
            model: stack.children.length
            delegate: Rectangle {
                height: 55
                width: root.width / stack.children.length;
                color: {
                    modelData === root.currentIndex
                        ? root.palette.button
                        : root.palette.base
                }
                Image {
                    source: stack.children[modelData].icon
                    width: 20
                    height: 20
                    smooth: true
                    y: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Flowee.Label {
                    text: stack.children[modelData].title
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    font.pixelSize: 14
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.currentIndex = modelData
                }
            }
        }
    }
}
