/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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

    background: Rectangle {
        color: palette.light
    }

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
        color: Pay.useDarkSkin ? palette.window : mainWindow.floweeBlue

        Flowee.HamburgerMenu {
            id: menuButton
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            wide: true
            x: 8
        }
        MouseArea {
            anchors.fill: menuButton
            // make the touch area a lot bigger, its safe as its limited to the 'header' area anyway.
            anchors.margins: -60
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
            visible: !portfolio.singleAccountSetup
            text:  portfolio.current.name
            color: "#fcfcfc"
            clip: true
            anchors.left: logo.right
            anchors.right: searchIcon.left
            anchors.margins: 12
            anchors.baseline: logo.baseline

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (accountSelector.visible)
                        accountSelector.close();
                    else
                        accountSelector.open();
                }
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

        AccountSelectorPopup {
            id: accountSelector
            width: root.width
            y: header.height

            onSelectedAccountChanged: portfolio.current = selectedAccount
        }
    }

    Item {
        id: stack
        width: root.width
        anchors.top: header.bottom; anchors.bottom: tabbar.top
    }

    Rectangle {
        anchors.fill: tabbar
        color: palette.window
    }

    Row {
        id: tabbar
        anchors.bottom: parent.bottom

        Repeater {
            model: stack.children.length
            delegate: Item {
                height: 65
                width: root.width / stack.children.length;

                Rectangle {
                    x: 5
                    height: 4
                    width: parent.width - 10
                    color: palette.highlight
                    visible: modelData === root.currentIndex
                }
                Rectangle {
                    anchors.fill: parent
                    color: palette.highlight
                    visible: modelData === root.currentIndex
                    opacity: 0.15
                }
                Image {
                    source: stack.children[modelData].icon
                    width: 30
                    height: 30
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

    /*
     * Fully encrypted wallet are useless to inspect or use until they are opened.
     * This is an overlay that covers the entire screen except for the header to make
     * unlocking a "modal" dialogue.
     */
    Rectangle {
        color: palette.window
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        visible: portfolio.current.needsPinToOpen && !portfolio.current.isDecrypted

        // eat all taps / clicks.
        MouseArea { anchors.fill: parent }

        // to avoid using resources in the 99% of the time the user is not unlocking his wallet,
        // we use a loader for the unlocking screen.
        Loader {
            id: unlockWalletWidget
            anchors.fill: parent
            source: parent.visible ? "./UnlockWalletPanel.qml" : ""
        }
    }
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            // when we are on another tab, we can go to 'main' on back.
            if (root.currentIndex != 0) {
                root.currentIndex = 0;
                event.accepted = true;
            }
        }
        // in all other cases, propagate the event up the stack.
    }
}
