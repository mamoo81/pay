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
import "../Flowee" as Flowee

Page {
    headerText: qsTr("Select Wallet")

    Flowee.Label {
        id: introText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        text: qsTr("Pick which wallet will be selected on starting Flowee Pay")
    }

    ListView {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: introText.bottom
        anchors.topMargin: 10
        clip: true
        model: portfolio.accounts
        delegate: Item {
            width: ListView.view.width
            height: column.height + 20

            Rectangle {
                anchors.fill: parent
                color: (index % 2) == 0 ? palette.base : palette.alternateBase
            }
            MouseArea {
                anchors.fill: parent
                onClicked: modelData.isPrimaryAccount = true;
            }
            Column {
                id: column
                y: 10
                x: 10
                width: parent.width - 20
                Flowee.Label {
                    id: mainText
                    text: modelData.name
                    font.bold: true
                }
                Flowee.Label {
                    text: {
                        if (modelData.allowInstaPay) {
                            var cur = Fiat.currencyName;
                            var limit = modelData.fiatInstaPayLimit(cur);
                            if (limit === 0)
                                return qsTr("No InstaPay limit set");
                            return qsTr("InstaPay till %1").arg(Fiat.formattedPrice(limit));
                        }
                        return qsTr("InstaPay is turned off");
                    }
                    font.pixelSize: mainText.pixelSize * 0.8
                    color: palette.brightText
                }
            }

            Flowee.Label {
                text: "✷";
                y: 10
                anchors.right: parent.right
                anchors.rightMargin: 5
                visible: modelData.isPrimaryAccount
            }
        }
    }
}
