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
import Flowee.org.pay

Item {
    id: addressInfo
    property QtObject info: null
    visible: info != null
    width: infoLabel.implicitWidth
    height: infoLabel.height

    // the outcome from a call to Pay.identifyString()
    required property var addressType;
    // set to true, if the address is usable for an actual payment
    property bool addressOk: (addressType === Wallet.CashPKH
            || addressType === Wallet.CashSH)
        || (paymentDetail.forceLegacyOk
            && (addressType === Wallet.LegacySH
                || addressType === Wallet.LegacyPKH));

    function createInfo() {
        if (addressOk) {
            info = Pay.researchAddress(paymentDetail.formattedTarget, addressInfo)
        }
        else {
            delete info;
            info = null;
        }
    }
    onAddressOkChanged: createInfo();

    Label {
        id: infoLabel
        anchors.right: parent.right
        font.italic: true
        text: {
            var info = addressInfo.info
            if (info == null)
                return "";
            if (portfolio.current.id === info.accountId)
                return qsTr("self", "payment to self")
            return info.accountName
        }
    }
}

