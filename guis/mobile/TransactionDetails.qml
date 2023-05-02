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
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;

Page {
    id: root
    property QtObject transaction: null
    property QtObject infoObject: null
    headerText: qsTr("Transaction Details")

    ColumnLayout {
        width: parent.width
        Flowee.LabelWithClipboard {
            id: txidLabel
            text: root.transaction == null ? "" : root.transaction.txid
            font.pixelSize: root.font.pixelSize * 0.8
            Layout.fillWidth: true
        }
        Flowee.Label {
            text: {
                if (root.transaction == null)
                    return ""
                var h = root.transaction.height;
                if (h === -2)
                    return qsTr("Rejected")
                if (h === -1)
                    return qsTr("Unconfirmed")
                return qsTr("Mined") + ": " + Pay.formatDateTime(root.transaction.date)
            }
        }
        Flowee.Label {
            text: qsTr("Coinbase")
            visible: root.transaction != null && root.transaction.isCoinbase
        }
        Flowee.Label {
            text: qsTr("CashFusion transaction")
            visible: root.transaction != null && root.transaction.isCashFusion
        }
        VisualSeparator {}
        Flowee.Label {
            text: qsTr("Size") + ":"
        }
        Flowee.Label {
            text: root.infoObject == null ? "" :
                    qsTr("%1 bytes").arg(root.infoObject.size)
        }
        /*
        VisualSeparator {}
        Flowee.Label {
            text: qsTr("Transaction comment") + ":"
        }
        QQC2.TextField {
            text: root.transaction == null ? "" : root.transaction.comment
            Layout.fillWidth: true
            onEditingFinished: root.transaction.comment = text
        } */
    }
}
