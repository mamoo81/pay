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
import QtQuick.Layouts

FocusScope {
    id: root
    property string icon: "qrc:/sending" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
    property string  title: qsTr("Send")

    ColumnLayout {
        width: parent.width - 20
        x: 10

        TextButton {
            text: qsTr("Scan a QR code")
            showPageIcon: true
            onClicked: thePile.push("PayWithQR.qml")
        }
        /*
        TextButton {
            text: qsTr("My Wallets")
            subtext: qsTr("Move between wallets")
            showPageIcon: true
            onClicked: thePile.push("MoveBetweeWallets.qml")
        } */
    }
}
