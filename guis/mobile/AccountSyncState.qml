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
import "../Flowee" as Flowee
import Flowee.org.pay;
import QtQuick.Shapes // for shape-path

Item {
    id: root
    height: indicator.height + 3 + (uptodate ? 0 : circleShape.height)
    property QtObject account: null
    property bool uptodate: false
    property int startPos: account.initialBlockHeight
    onAccountChanged: checkIfDone();
    function checkIfDone() {
        let startPos = account.initialBlockHeight;
        if (startPos === -2) {
            // special number, the account is just created and waits for transactions.
            uptodate = true;
            return;
        }
        let end = Pay.expectedChainHeight;
        let currentPos = account.lastBlockSynched;
        // only show the progress-circle when its more than 2 days behind
        // and we are not synched
        uptodate = end - startPos < 300 || end - currentPos <= 6;
    }

    Connections {
        target: account
        function onLastBlockSynchedChanged() { checkIfDone(); }
    }

    // The 'progress' circle.
    Shape {
        id: circleShape

        property int goalHeight: Pay.expectedChainHeight
        visible: !root.uptodate
        anchors.horizontalCenter: parent.horizontalCenter
        width: 200
        height: 100
        antialiasing: true
        smooth: true
        ShapePath {
            strokeColor: mainWindow.floweeBlue
            strokeWidth: 3
            fillColor: mainWindow.floweeGreen
            startX: 0; startY: 100

            PathAngleArc {
                id: progressCircle
                centerX: 100
                centerY: 100
                radiusX: 100; radiusY: 100
                startAngle: -180
                sweepAngle: {
                    let end = circleShape.goalHeight;
                    let startPos = root.startPos;
                    if (startPos == 0)
                        return 0;
                    let currentPos = root.account.lastBlockSynched;
                    let totalDistance = end - startPos;
                    if (totalDistance <= 6)
                        return 180; // uptodate
                    let ourProgress = currentPos - startPos;
                    return 180 * (ourProgress / totalDistance);
                }
                Behavior on sweepAngle {  NumberAnimation { duration: 50 } }
            }
            PathLine { x: 100; y: 100 }
            PathLine { x: 0; y: 100 }
        }
    }
    Flowee.Label {
        id: indicator
        width: parent.width
        y: root.uptodate ? 0 : circleShape.height + 3
        wrapMode: Text.Wrap
        text: {
            if (isLoading)
                return "";
            var account = portfolio.current;
            if (account === null)
                return "";
            if (account.needsPinToOpen && !account.isDecrypted)
                return qsTr("Status: Offline");
            return account.timeBehind;
        }
        font.italic: true
        Behavior on y {  NumberAnimation { duration: 100 } }
    }
    Behavior on height {  NumberAnimation { duration: 100 } }
}
