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

ConfigItem {
    id: root
    property QtObject account: null

    property QtObject detailsAction: Action {
        text: qsTr("Details")
        onTriggered: {
            portfolio.current = root.account;
            accountOverlay.state = "accountDetails";
        }
    }
    property QtObject archiveAction: Action {
        text: root.account.isArchived ? qsTr("Unarchive") : qsTr("Archive Wallet")
        onTriggered: root.account.isArchived = !root.account.isArchived
    }
    property QtObject primaryAction: Action {
        enabled: !root.account.isDefaultWallet
        text: enabled ? qsTr("Make Primary") : qsTr("★ Primary")
        onTriggered: root.account.isDefaultWallet = !root.account.isDefaultWallet
    }
    property QtObject encryptAction: Action {
        text: qsTr("Protect With Pin...")
        onTriggered: {
            portfolio.current = root.account;
            accountOverlay.state = "startWalletEncryption";
        }
    }
    property QtObject openWalletAction: Action {
        text: qsTr("Open", "Open encrypted wallet")
        onTriggered: tabbar.currentIndex = 0
    }

    property QtObject closeWalletAction: Action {
        text: qsTr("Close", "Close encrypted wallet")
        onTriggered: root.account.closeWallet();
    }

    onAboutToOpen: {
        var items = [];
        items.push(detailsAction);
        var encrypted = root.account.needsPinToOpen;
        var decrypted = root.account.isDecrypted;
        if ((encrypted || root.account.needsPinToPay) && decrypted)
            items.push(closeWalletAction);
        if (encrypted && !decrypted && tabbar.currentIndex != 0)
            items.push(openWalletAction);
        var singleAccountSetup =portfolio.singleAccountSetup
        var isArchived = root.account.isArchived;
        if (!singleAccountSetup && !isArchived)
            items.push(primaryAction);
        if (!encrypted)
            items.push(encryptAction);
        if (!singleAccountSetup)
            items.push(archiveAction);
        setMenuActions(items);
    }
}
