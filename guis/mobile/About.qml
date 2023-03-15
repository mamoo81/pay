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
import QtQuick.Layouts
import "../Flowee" as Flowee

Page {
    headerText: qsTr("About")

    ColumnLayout {
        width: parent.width

        Image {
            source: Pay.useDarkSkin ? "qrc:/FloweePay-light.svg" : "qrc:/FloweePay.svg"
            Layout.fillWidth: true
            fillMode: Image.PreserveAspectFit
        }

        Flowee.Label {
            text: "Flowee Pay (mobile) v" + Application.version
        }
        Item { width: 10; height: 10 } // spacer

        TextButton {
            id: translate
            text: qsTr("Help translate this app")
            imageSource: Pay.useDarkSkin ? "qrc:/external-light.svg" : "qrc:/external.svg"
            onClicked: Qt.openUrlExternally("https://crowdin.com/project/floweepay");
        }
        TextButton {
            text: qsTr("License")
            imageSource: translate.imageSource
            onClicked: Qt.openUrlExternally("https://www.gnu.org/licenses/gpl-3.0")
        }
        TextButton {
            text: qsTr("Credits")
            subtext: qsTr("© 2020-2023 Tom Zander and contributors")
            showPageIcon: true
            onClicked: thePile.push(creditsPage)

            Component {
                id: creditsPage
                Page {
                    headerText: qsTr("Credits")

                    Flowee.Label {
                        text: "## Author and maintainer
Tom Zander

## Code Contributors

You?

## Art Contributors

You?

## Beta Testers

fshinetop

## Translations

Nederland<dl>
<dt>s</dt>
<dd>Tom Zander</dd>
<dt>Polski</dt>
<dd>yantri</dd>
</dl>
"
                        textFormat: Text.MarkdownText
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }
        }
        TextButton {
            text: qsTr("Project Home")
            subtext: qsTr("With git repository and issues tracker")
            imageSource: translate.imageSource
            onClicked: Qt.openUrlExternally("https://codeberg.org/Flowee/pay");
        }
        TextButton {
            text: qsTr("Telegram")
            imageSource: translate.imageSource
            onClicked: Qt.openUrlExternally("https://t.me/Flowee_org");
        }
    }
}
