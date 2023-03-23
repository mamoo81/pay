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
import Flowee.org.pay;
import "../Flowee" as Flowee

Item {
    id: root
    height: indicator.height + 3 + (uptodate ? 0 : progressbar.height + 10)
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

    // The progressbar
    Rectangle {
        id: progressbar
        y: 10 // top spacing of widget, only when doing progress report
        x: 10
        width: parent.width - 20
        height: 40 // percentLabel.height + 10
        color: "#00000000"
        border.width: 2
        border.color: palette.midlight
        radius: 10
        visible: !root.uptodate

        property double progress: {
            let startPos = root.startPos;
            let end = Pay.expectedChainHeight
            if (startPos == 0)
                return 0;
            let currentPos = root.account.lastBlockSynched;
            let totalDistance = end - startPos;
            if (totalDistance <= 6)
                return 100; // uptodate
            let ourProgress = currentPos - startPos;
            return ourProgress / totalDistance;
        }

        Rectangle {
            width: Math.min(10, parent.width * parent.progress)
            y: 2
            height: parent.height - 4
            color: palette.highlight
            radius: 10
        }

        Rectangle {
            id: bar
            x: 5
            y: 2
            width: {
                var full = progressbar.width;
                var w = full * progressbar.progress
                w = Math.min(w, full - 5) // the right max
                w -= 5; // our X offset
                Math.max(0, w);
            }
            height: parent.height - 4
            color: palette.highlight
            clip: true
            Flowee.Label {
                id: percentLabel
                text: (100 * progressbar.progress).toFixed(0) + "%"
                y: 5
                x: progressbar.width / 2 - 10 - width / 2
                color: palette.highlightedText
            }
        }
        Rectangle {
            width: {
                // just the last 10 pixels.
                var full = progressbar.width;
                return Math.max(0, full * progressbar.progress - (full - 10));
            }
            x: progressbar.width - 10
            y: 2
            height: parent.height - 4
            color: palette.highlight
            radius: 10
        }
        Rectangle {
            id: groove
            y: 2
            height: parent.height - 4
            color: palette.light
            clip: true
            x: {
                var full = progressbar.width;
                var distance = progressbar.progress * full;
                distance = Math.min(distance, full - 5); // and right
                distance = Math.max(5, distance); // avoid the left rounding area 
                return distance;
            }
            width: {
                var full = progressbar.width;
                var w = full - progressbar.progress * full;
                w -= 5;
                w = Math.min(w, full - 10); // the rounded corners
                return w;
            }
            Flowee.Label {
                text: percentLabel.text
                y: 5
                x: progressbar.width / 2 - 5 - width / 2 - parent.x
                // color: palette.highlightedText
            }
        }
    }
    Flowee.Label {
        id: indicator
        width: parent.width
        y: root.uptodate ? 0 : progressbar.height + 13
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
