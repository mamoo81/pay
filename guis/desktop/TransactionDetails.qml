/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2024 Tom Zander <tom@flowee.org>
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

QQC2.ApplicationWindow {
    id: root
    visible: false
    minimumWidth: 200
    minimumHeight: 200
    width: 400
    height: 500
    title: qsTr("Transaction Details")
    modality: Qt.NonModal
    flags: Qt.Dialog

    function openTab(walletIndex) {
console.log("xxx");
        tabs.push(portfolio.current.txInfo(walletIndex, root))
    }
    property var tabs: []

    Flowee.TabBar {
        anchors.fill: parent

        Repeater {
            model: root.tabs
            delegate: Item {
                property string title: "bla"
                property string icon: "bla"
                Rectangle {
                    width: 100
                    height: 10
                    color: "red"
                }
            }
        }
    }

    Flowee.Label {
        y: 50
        text: tabs.length
    }
}
