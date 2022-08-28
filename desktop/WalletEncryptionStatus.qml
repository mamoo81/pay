/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
import QtQuick 2.11
import QtQuick.Controls 2.11

Item {
    id: root
    height: encryptionStatusLabel.height
    visible: account.needsPinToPay || account.needsPinToOpen

    property QtObject account: null

    Image {
        id: lockIcon
        source: Pay.useDarkSkin ? "qrc:/lock-light.svg" : "qrc:/lock-dark.svg"
        height: parent.height + 2
        width: height
        opacity: root.account.isDecrypted ? 0.65 : 1
        y: -3
    }
    Label {
        id: encryptionStatusLabel
        wrapMode: Text.WordWrap
        font.italic: true
        anchors.left: lockIcon.right
        anchors.leftMargin: 3
        text: {
            var txt = "";
            if (root.account.needsPinToPay)
                txt = qsTr("Pin to Pay");
            else if (root.account.needsPinToOpen)
                txt = qsTr("Pin to Open");
            if (root.account.isDecrypted)
                txt += " " + qsTr("(Opened)", "Wallet is decrypted");
            return txt;
        }
    }
}
