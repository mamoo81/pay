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
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import Flowee.org.pay

FocusScope {
    id: root
    // if true, the bitcoin value is provided by the user (or QR), and the fiat follows.
    // if false, the user edits the fiat price and the bitcoin value is calculated.
    // Notice that 'send all' overrules both and gets the data from the wallet-total
    property bool fiatFollowsSats: false
    // made available for the NumericKeyboardWidget
    property var editor: fiatFollowsSats ? priceBch.money : priceFiat.money;
    /**
     * Payment Backend processes the fiat and satoshi based pair of payments.
     * The default is a simple backend, notice that the Payment QML type
     * actually provides its own backend(s), as each output is one.
     */
    property QtObject paymentBackend : PaymentBackend {}

    onFiatFollowsSatsChanged: {
        if (!activeFocus)
            return;
        if (fiatFollowsSats)
            priceBch.forceActiveFocus();
        else
            priceFiat.forceActiveFocus();
    }

    function shake() {
        if (fiatFollowsSats)
            bchShaker.start();
        else
            fiatShaker.start();
    }

    implicitHeight: 140
    height: implicitHeight

    Flowee.BitcoinValueField {
        id: priceBch
        y: root.fiatFollowsSats ? 5 : 68
        value: paymentBackend.paymentAmount
        focus: true
        fontPixelSize: size
        property double size: fiatFollowsSats ? 38 : mainWindow.font.pixelSize* 0.8
        onActiveFocusChanged: if (activeFocus) fiatFollowsSats = true
        Behavior on size { NumberAnimation { } }
        Behavior on y { NumberAnimation { } }
        Flowee.ObjectShaker { id: bchShaker } // 'shake' to give feedback on mistakes

        // this unchecks 'max' on user editing of the value
        onValueEdited: paymentBackend.paymentAmount = value
    }
    MouseArea {
        /* Since the valueField is centred but only allows clicking on its active surface,
          we provide this one to make it possible to click on the full width and activate it.
        */
        width: root.width
        height: priceBch.height
        y: priceBch.y
        onClicked: priceBch.forceActiveFocus();
    }

    Flowee.FiatValueField  {
        id: priceFiat
        value: paymentBackend.paymentAmountFiat
        y: root.fiatFollowsSats ? 68 : 5
        focus: true
        fontPixelSize: size
        property double size: !fiatFollowsSats ? 38 : mainWindow.font.pixelSize * 0.8
        onActiveFocusChanged: if (activeFocus) fiatFollowsSats = false
        Behavior on size { NumberAnimation { } }
        Behavior on y { NumberAnimation { } }
        Flowee.ObjectShaker { id: fiatShaker }
        onValueEdited: paymentBackend.paymentAmountFiat = value
    }

    Rectangle {
        id: inputChooser
        y: 100
        height: 40
        anchors.horizontalCenter: parent.horizontalCenter
        border.width: 1
        border.color: palette.midlight
        color: palette.light
        width: inputs.width + 20
        radius: 15

        Rectangle {
            color: palette.highlight
            opacity: 0.3
            radius: 6
            width: 35
            height: parent.height - 4
            y: 2
            x: root.fiatFollowsSats ? 5 : 45

            Behavior on x { NumberAnimation { } }
        }

        Row {
            id: inputs
            x: 10
            y: 7.5
            height: parent.height
            spacing: 4
            Image {
                id: logo
                source: "qrc:/bch.svg"
                width: 25
                height: 25
                smooth: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: priceBch.forceActiveFocus();
                }
            }
            Item { width: 8; height: 1 } // spacer

            Flowee.Label {
                text: (Fiat.currencySymbolPost + Fiat.currencySymbolPrefix).trim()
                width: 24
                font.pixelSize: 32
                horizontalAlignment: Text.AlignRight
                anchors.baseline: logo.bottom

                MouseArea {
                    anchors.fill: parent
                    onClicked: priceFiat.forceActiveFocus();
                }
            }
            Rectangle {
                width: 1
                y: inputs.y * -1
                height: parent.height
                color: palette.dark
            }

            Flowee.HamburgerMenu {
                y: 6
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -12
                    anchors.rightMargin: -25
                    onClicked: languageMenu.open()
                }

                Loader {
                    id: languageMenu
                    function open() {
                        sourceComponent = languageMenuComponent
                    }
                    onLoaded: item.open();
                }

                Component {
                    id: languageMenuComponent

                    QQC2.Menu {
                        Repeater {
                            model: Pay.recentCountries()
                            QQC2.MenuItem {
                                text: {
                                    var loc = Qt.locale(modelData);
                                    return loc.currencySymbol(Locale.CurrencySymbol) + " "
                                            + loc.currencySymbol(Locale.CurrencyDisplayName);
                                }
                                onClicked: Pay.setCountry(modelData);
                            }
                        }
                        QQC2.MenuItem {
                            text: qsTr("All Currencies")
                            onClicked: thePile.push("./CurrencySelector.qml")
                        }

                        onVisibleChanged: if (!visible) languageMenu.sourceComponent = undefined; // unload us
                    }
                }
            }
            Item { width: 5; height: 1 } // spacer
        }
    }
}

