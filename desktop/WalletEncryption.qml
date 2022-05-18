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
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "widgets" as Flowee

Item {
    id: root
    property QtObject account: portfolio.current
    focus: true

    Flowee.CloseIcon {
        id: closeIcon
        y: 30
        anchors.margins: 6
        anchors.right: parent.right
        onClicked: accountOverlay.state = "showTransactions"
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        // anchors.bottom: parent.bottom
        anchors.margins: 10
        spacing: 10
        Label {
            id: stateLabel
            text: {
                var isOpen = root.account.isDecrypted ? " (opened)" : "";
                if (root.account.needsPinToPay)
                    return "Has been light-encrypted" + isOpen;
                if (root.account.needsPinToOpen)
                    return "Has been fully-encrypted" + isOpen;
                return "unencrypted"
            }
            color: "white"
        }

        Flowee.TextField {
            id: password
        }
        Flowee.Button {
            text: "Pin to Pay"
            enabled: !root.account.needsPinToPay && !root.account.needsPinToOpen && password.text.length > 3
            onClicked: {
                root.account.encryptPinToPay(password.text);
                password.text = ""
            }
        }
        Flowee.Button {
            text: "Pin to Open"
            enabled: !root.account.needsPinToOpen && password.text.length > 3
            onClicked: {
                root.account.encryptPinToOpen(password.text);
                password.text = ""
            }
        }
        Flowee.Button {
            text: "Open wallet"
            enabled: (root.account.needsPinToPay || root.account.needsPinToOpen) && password.text.length > 1
            onClicked: {
                root.account.decrypt(password.text)
                if (root.account.isDecrypted)
                    password.text = ""
            }
        }
        Flowee.Button {
            text: "Close wallet"
            enabled: (root.account.needsPinToPay || root.account.needsPinToOpen) &&  root.account.isDecrypted
            onClicked: root.account.closeWallet()
        }
    }
}
