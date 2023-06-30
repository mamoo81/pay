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
import "../ControlColors.js" as ControlColors
import "../Flowee" as Flowee
import Flowee.org.pay;

import QtQuick.Controls.Basic

ApplicationWindow {
    id: mainWindow
    title: "Flowee Pay"
    width: 360
    height: 720
    minimumWidth: 300
    minimumHeight: 400
    visible: true
    onVisibleChanged: if (visible) ControlColors.applySkin(mainWindow)

    property bool isLoading: typeof portfolio === "undefined";
    onIsLoadingChanged: {
        // only load our UI when the p2p layer is loaded and all
        // variables are available.
        if (!isLoading) {
            thePile.replace("./MainView.qml");
            if (!portfolio.current.isUserOwned)
                thePile.push("./StartupScreen.qml");
        }
    }
    Component.onCompleted: updateFontSize();
    function updateFontSize() {
        // 75% = > 14.25,  100% => 19,  200% => 28
        mainWindow.font.pixelSize = 17 + (11 * (Pay.fontScaling-100) / 100)
    }
    Connections {
        target: Pay
        function onFontScalingChanged() { updateFontSize(); }
    }

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"
    property color errorRed: Pay.useDarkSkin ? "#ff6568" : "#940000"
    property color errorRedBg: Pay.useDarkSkin ? "#671314" : "#9f1d1f"

    StackView {
        id: thePile
        anchors.fill: parent
        initialItem: Pay.appProtection === FloweePay.AppPassword ? "./UnlockApplication.qml" : "./Loading.qml";
        onCurrentItemChanged: if (currentItem != null) currentItem.takeFocus();
        enabled: !menuOverlay.open

        Keys.onPressed: (event)=> {
            if (event.key === Qt.Key_Back) {
                // We eat this and have nothing happen.
                // This is useful to avoid the app closing on pressing 'back' one time too many on Android
                event.accepted = true;
            }
        }
    }
    MenuOverlay {
        id: menuOverlay
        anchors.fill: parent
    }

    QRScannerOverlay {
        anchors.fill: parent
    }
    QQC2.Popup {
        id: notificationPopup
        y: 110
        x: 25
        width: mainWindow.contentItem.width - 50
        height: label.implicitHeight * 3
        dim: true
        property alias text: label.text

        function show(message) {
            if (message === "")
                return;
            text = message;
            visible = true;
        }
        Flowee.Label {
            id: label
            width: parent.width - 12
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
        Rectangle {
            id: timeoutBar
            width: 5
            height: visible ? parent.height - 12 : 1
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 6
            color: palette.highlight

            Behavior on height { NumberAnimation { duration: notificationPopupTimer.interval } }
        }

        background: Rectangle {
            color: palette.light
            border.color: palette.midlight
            border.width: 1
            radius: 5
        }
        Overlay.modeless: Rectangle {
            color: Pay.useDarkSkin ? "#33000000" : "#33ffffff"
        }

        Timer {
            id: notificationPopupTimer
            running: parent.visible;
            interval: 6000
            onTriggered: notificationPopup.visible = false;

        }
    }
}
