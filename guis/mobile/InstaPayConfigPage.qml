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
// import QtQuick.Layouts
import "../Flowee" as Flowee

Page {
    id: root
    headerText: qsTr("Instant Pay")

    property QtObject account: portfolio.current

    Flowee.Label {
        id: introText
        text: qsTr("Requests for payment can be approved automatically using Instant Pay.")
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 10
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Rectangle {
        anchors.top: introText.bottom
        anchors.topMargin: 10
        width: parent.width
        radius: 10
        height: warning.implicitHeight + 20
        visible: root.account.needsPinToPay
        color: "#00000000"
        border.width: 2
        border.color: "blue"
        Flowee.Label {
            x: 10
            y: 10
            width: parent.width - 20
            id: warning
            text: qsTr("Protected wallets can not be used for InstaPay because they require a PIN before usage")
        }
    }

    Flowee.CheckBox {
        id: mainToggle
        anchors.top: introText.bottom
        anchors.topMargin: 10
        width: parent.width
        text: qsTr("Enable Instant Pay for this wallet")
        checked: root.account.allowInstaPay
        onClicked: root.account.allowInstaPay = checked
    }

    PageTitledBox {
        id: priceInput
        title: qsTr("Maximum Amount")
        anchors.top: mainToggle.bottom
        anchors.topMargin: 40
        width: parent.width
        enabled: mainToggle.checked
        property string currency: Fiat.currencyName
        property int instaPayLimit: root.account.fiatInstaPayLimit(currency);
        property alias editor: priceFiat.money
        function shake() { shaker.start(); }

        Item {
            width: parent.width
            Flowee.Label {
                text: Fiat.currencyName
                font.pixelSize: 20
                color: enabled ? palette.windowText : palette.brightText
            }
            Flowee.FiatValueField  {
                id: priceFiat
                value: priceInput.instaPayLimit
                focus: true
                Flowee.ObjectShaker { id: shaker }
                onValueEdited: root.account.setFiatInstaPayLimit(priceInput.currency, value);
                fontPixelSize: 20
                color: enabled ? palette.windowText : palette.brightText
            }
        }
    }

    NumericKeyboardWidget {
        id: numericKeyboard
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        width: parent.width
        enabled: mainToggle.checked
        dataInput: priceInput
    }



    /*
      List currencies a limit has been set for.
    */
}
