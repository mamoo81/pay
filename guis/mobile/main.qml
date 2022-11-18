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

import QtQuick.Controls.Basic

ApplicationWindow {
    id: mainWindow
    title: "Flowee Pay"
    width: 360
    height: 720
    visible: true
    onVisibleChanged: if (visible) ControlColors.applySkin(mainWindow)

    property bool isLoading: typeof portfolio === "undefined";
    onIsLoadingChanged: {
        // only load our UI when the p2p layer is loaded and all
        // variables are available.
        if (!isLoading)
            thePile.replace("./MainView.qml");
    }
    Component.onCompleted: {
        var scale = Pay.fontScaling;
        mainWindow.font.bold = scale > 90;
        mainWindow.font.fontPtSize = scale > 90;
        var baseFromOS = mainWindow.font.pointSize;
        mainWindow.font.pointSize = baseFromOS + 2 * (scale / 100)
    }

    property color floweeSalmon: "#ff9d94"
    property color floweeBlue: "#0b1088"
    property color floweeGreen: "#90e4b5"

    StackView {
        id: thePile
        anchors.fill: parent
        initialItem: "./Loading.qml";
    }
    MenuOverlay {
        id: menuOverlay
        anchors.fill: parent
    }

    /*
    Item {
        id: touchFeedbackOverlay
        anchors.fill: parent

        Rectangle {
            id: clickFeedback
            color: mainWindow.floweeGreen
            opacity: 0.3
            width: 70
            height: 70
            radius: 35
            visible: opacity > 0

            Rectangle {
                width: 7
                height: 7
                anchors.centerIn: parent
            }

            Timer {
                id: fadeTimer
                running: parent.visible
                interval: 50
                repeat: true
                onTriggered: clickFeedback.opacity = 0
            }
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onClicked: (event) => {
                event.accepted = false;
                fadeTimer.stop();
                clickFeedback.x = event.x - clickFeedback.width / 2;
                clickFeedback.y = event.y - clickFeedback.height / 2;
                clickFeedback.opacity = 0.4
                fadeTimer.start();
            }
        }
    }
    */
}
