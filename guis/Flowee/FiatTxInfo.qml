/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2024 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee

GridLayout {
    id: root
    columns: 2

    required property QtObject txInfo
    // Is this transaction a 'move between addresses' tx.
    // This is a heuristic and not available in the model, which is why its in the view.
    property bool isMoved: {
        if (txInfo === null)
            return false;
        if (txInfo.isCoinbase || txInfo.isFused
                || txInfo.fundsIn === 0)
            return false;
        var amount = txInfo.fundsOut - txInfo.fundsIn
        return amount < 0 && amount > -2500 // then the diff is likely just fees.
    }
    property double amountBch: {
        if (txInfo === null)
            return 0;
        if (isMoved)
            return txInfo.fundsIn
        return txInfo.fundsOut - txInfo.fundsIn;
    }

    visible: {
        if (txInfo === null)
            return false;
        if (txInfo.minedHeight < 1)
            return false;
        if (txInfo.isFused)
            return false;
        if (isMoved)
            return false;
        if (valueThenLabel.fiatPrice === 0)
            return false;
        if (Math.abs(amountBch) < 10000) // hardcode 10k sats here, may need adjustment later
            return false;
        return true;
    }

    Flowee.Label {
        text: qsTr("Value now") + ":"
    }
    Flowee.Label {
        text: {
            if (txInfo === null || txInfo.minedHeight <= 0)
                return "";
            var fiatPriceNow = Fiat.price;
            var gained = (fiatPriceNow - valueThenLabel.fiatPrice) / valueThenLabel.fiatPrice * 100
            return Fiat.formattedPrice(Math.abs(root.amountBch), fiatPriceNow)
                    + " (" + (gained >= 0 ? "↑" : "↓") + Math.abs(gained).toFixed(2) + "%)";
        }
    }

    // price at mining
    // value in exchange gained
    Flowee.Label {
        id: priceAtMining
        text: qsTr("Value then") + ":"
    }
    Flowee.Label {
        Layout.fillWidth: true
        id: valueThenLabel
        // when the backend does NOT get an 'accurate' (timewise) value,
        // it returns zero. Which makes us set visibility to false
        property int fiatPrice: {
            if (txInfo == null)
                return 0;
            Fiat.historicalPriceAccurate(txInfo.date)
        }
        text: Fiat.formattedPrice(Math.abs(root.amountBch), fiatPrice)
    }
}
