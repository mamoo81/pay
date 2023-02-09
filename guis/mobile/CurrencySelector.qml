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
import "../Flowee" as Flowee
import Flowee.org.pay;

Page {
    id: root
    headerText: qsTr("Select Currency")
    ListView {
        anchors.fill: parent
        model: [
            "nl_NL",
            "en_US",
            "ar_AE", // "aed" United Arab Emirates dirham
            "es_AR", // "ars" Argentine peso
            "en_AU", // "aud"  Australian dollar
            "bn_BD", // "bdt" Bangladeshi taka
            "ar_BH", // "bhd" Bahraini dinar
            "en-BM", // "bmd" Bermudian dollar
            "pt_BR", // "brl" Brazilian real
            "en_CA", // "cad" Canada
            "de_CH", // "chf" Switzerland
            "es_CL", // "clp" Chilean peso
            "zh", // "cny"
            "cs", // "czk"
            "da", // "dkk"
            "en_GG", // "gbp"
            "zh_HK", // "hkd"
            "hu", // "huf"
            "id", // "idr"
            "he", // "ils"
            "hi", // "inr"
            "ja", // "jpy"
            "ko", // "krw"
            "ar_KW", // "kwd"
            "si", // "lkr"
            "my", // "mmk"
            "es_MX", // "mxn"
            "ms", // "myr"
            "cch", // "ngn"
            "no_NO", // "nok"
            "en_PN", // "nzd"
            "fil", // "php"
            "pl_PL", // pln
            "ur", // "pkr"
            "ru", // "rub"
            "ar_SA", // "sar"
            "sv", // "sek"
            "zh_SG", // "sgd"
            "th", // "thb"
            "tr", // "try"
            "zh_TW", // "twd"
            "uk", // "uah"
            "es_VE", // "vef"
            "vi", // "vnd"
            "en_LS", // "zar"
        ]

        delegate: Rectangle {
            width: ListView.view.width
            height: label.height + 20
            color: (index % 2) == 0 ? root.palette.base : root.palette.alternateBase

            Flowee.Label {
                id: iso
                y: 10
                text: Qt.locale(modelData).currencySymbol(Locale.XCurrencyIsoCode)
            }

            Flowee.Label {
                id: label
                y: 10
                anchors.left: iso.right
                anchors.leftMargin: 10
                anchors.right: parent.right
                text: {
                    var loc = Qt.locale(modelData);
                    return "(" + loc.currencySymbol(Locale.CurrencySymbol) + ") "
                            + loc.currencySymbol(Locale.CurrencyDisplayName);
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    Pay.setCountry(modelData);
                    thePile.pop();
                }
            }
        }

    }
}
