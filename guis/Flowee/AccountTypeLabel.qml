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

/// Simple label which will show as text the type of account provided it.
Label {
    id: root

    property QtObject account: null

    wrapMode: Text.WordWrap
    visible: root.account.isDecrypted || root.account.needsPinToPay
    font.italic: true
    text: {
        if (root.account.isSingleAddressAccount)
            return qsTr("This wallet is a single-address wallet.")
        if (root.account.isHDWallet)
            return qsTr("This wallet is based on a HD seed-phrase")
        // ok we only have one more type so far, so this is rather simple...
        return qsTr("This wallet is a simple multiple-address wallet.")
    }
}
