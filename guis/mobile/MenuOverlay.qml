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
import "../ControlColors.js" as ControlColors
import "../Flowee" as Flowee

Item {
    id: root
    property bool open: false

    onOpenChanged: if (!open) baseArea.openAccounts = false; // close the accounts when the menu is closed

    Rectangle {
        anchors.fill: parent
        opacity: {
            if (!root.open)
                return 0;
            // we become 50% opaque when the menuArea is fully open (x == 0)
            return (menuArea.x + 250) / 500;
        }
        color: "black"
    }

    Rectangle {
        id: menuArea
        color: palette.window
        width: 300
        height: parent.height
        x: root.open ? 0 : 0 - width -3
        clip: true

        Rectangle {
            id: baseArea
            width: parent.width
            height: {
                var h = logo.height + 20;
                // if its opened
                if (openAccounts)
                    h = h + extraOptions.height + 10
                // but we just don't show the accounts at all if
                // this is the initial (empty) wallet.
                if (!isLoading && portfolio.accounts.length > 1)
                    h = h+ Math.max(currentAccountName.height, 12) + 10
                return h;
            }
            color: Qt.lighter(palette.window)
            property bool openAccounts: false
            clip: true

            Rectangle {
                id: logo
                width: 70
                height: 70
                x: 5
                y: 5
                radius: 35
                color: mainWindow.floweeBlue
                Item {
                    // clip the logo only, ignore the text part
                    width: 50
                    height: 50
                    clip: true
                    x: 13
                    y: 16
                    Image {
                        source: "qrc:/FloweePay-light.svg"
                        // ratio: 449 / 77
                        width: height / 77 * 449
                        height: 50
                    }
                }
            }

            Image {
                width: 25
                height: 25
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 10
                source: Pay.useDarkSkin ? "qrc:/maslenica.svg" : "qrc:/moon.svg"
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -10
                    onClicked: {
                        Pay.useDarkSkin = !Pay.useDarkSkin
                        ControlColors.applySkin(mainWindow)
                    }
                }
            }
            Flowee.Label {
                id: currentAccountName
                x: 10
                y: logo.height + 20
                text: {
                    if (mainWindow.isLoading)
                        return ""
                    return portfolio.current.name
                }
            }
            Image {
                id: openButton
                source: Pay.useDarkSkin ? "qrc:/smallArrow-light.svg" : "qrc:/smallArrow.svg"
                rotation: baseArea.openAccounts ? 180 : 0
                Behavior on rotation { NumberAnimation {} }
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.bottom: currentAccountName.bottom
                anchors.bottomMargin: 8
                width: 20
                height: 7
            }
            MouseArea {
                anchors.top: currentAccountName.top
                anchors.bottom: openButton.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: -10
                onClicked: baseArea.openAccounts = !baseArea.openAccounts
            }

            ColumnLayout {
                id: extraOptions
                width: baseArea.width - 20
                y: logo.height + 20 + (currentAccountName.visible ? currentAccountName.height + 10 : 0)
                x: 10
                Repeater { // portfolio holds all our accounts
                    width: parent.width
                    model: mainWindow.isLoading ? 0 : portfolio.accounts
                    TextButton {
                        text: modelData.name
                        visible: portfolio.current !== modelData
                        onClicked: {
                            portfolio.current = modelData
                            baseArea.openAccounts = false
                        }
                    }
                }
                Loader {
                    width: parent.width
                    active: !isLoading && portfolio.accounts.length > 1
                    sourceComponent: addWalletRow
                }
            }

            Behavior on height { NumberAnimation { } }
        }

        Flickable {
            anchors.top: baseArea.bottom
            anchors.topMargin: 10
            anchors.bottom: versionLabel.top
            contentHeight: contentLayout.height
            width: parent.width - 20
            clip: true
            boundsBehavior: Flickable.StopAtBounds // don't show this is a flickable if there is no space issues
            x: 10
            ColumnLayout {
                id: contentLayout
                width: parent.width
                Repeater {
                    model: MenuModel
                    TextButton {
                        text: model.name
                        showPageIcon: true
                        onClicked: {
                            var target = model.target
                            if (target !== "") {
                                thePile.push(model.target)
                                root.open = false;
                            }
                        }
                    }
                }
                Loader {
                    width: parent.width
                    active: isLoading || portfolio.accounts.length <= 1
                    sourceComponent: addWalletRow
                }
            }
        }

        Flowee.Label {
            id: versionLabel
            x: 10
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            text: "Flowee Pay (mobile) v" + Application.version
            font.pointSize: mainWindow.font.pointSize * 0.9
            font.bold: false
            wrapMode: Text.Wrap
            width: parent.width - 20
        }

        Behavior on x { NumberAnimation { duration: 100 } }

        property bool opened: false
        onXChanged: {
            if (!root.open)
                opened = false;
            else if (x === 0)
                opened = true;
            // close on user drag to the left
            if (opened && x < -50)
                root.open = false
        }
        // gesture (swipe right) to close menu
        DragHandler {
            id: dragHandler
            enabled: root.open
            yAxis.enabled: false
            xAxis.minimum: -200
            xAxis.maximum: 0

            onActiveChanged: {
                // should the user abort the swipe left, restore
                // the original binding
                if (!active && root.open)
                    menuArea.x = root.open ? 0 : 0 - width -3
            }
        }
    }
    // allow close by clicking next to the menu
    MouseArea {
        width: parent.width - menuArea.width
        height: parent.height
        anchors.right: parent.right
        enabled: root.open
        onClicked: root.open = false;
    }

    Component {
        id: addWalletRow
        Item {
            height: addWalletButton.height
            Rectangle {
                id: horizontalBar
                width: 13
                height: 2
                x: 2
                color: palette.mid
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                id: verticalBar
                width: 2
                height: 13
                anchors.horizontalCenter: horizontalBar.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: palette.mid
            }
            TextButton {
                id: addWalletButton
                text: qsTr("Add Wallet")
                showPageIcon: true
                anchors.left: horizontalBar.right
                anchors.leftMargin: 6
                anchors.right: parent.right
                onClicked: {
                    thePile.push("./NewAccount.qml")
                    root.open = false
                    baseArea.openAccounts = false
                }
            }
        }
    }
}
