/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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

TextButton {
    id: root
    property QtObject account: portfolio.current

    text: root.account.allowsInstaPay ? qsTr("Enable Instant Pay") : qsTr("Configure Instant Pay")
    property int limit: 0

    function updateLimit() {
        limit = root.account.fiatInstaPayLimit(Fiat.currencyName);
    }
    Component.onCompleted: updateLimit();
    Connections {
        target: root.account
        function onInstaPayLimitChanged() {
            updateLimit();
        }
    }

    subtext: {
        if (!root.account.allowInstaPay)
            return qsTr("Fast payments for low amounts")
        if (limit === 0)
            return qsTr("Not configured");
        return qsTr("Limit set to: %1").arg(Fiat.formattedPrice(limit));
    }

    showPageIcon: true
    onClicked: {
        var newPage = thePile.push("./InstaPayConfigPage.qml")
        newPage.account = root.account;
    }
}
