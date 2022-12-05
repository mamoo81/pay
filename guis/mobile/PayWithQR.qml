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
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;

Page {
    headerText: qsTr("Scan QR")

    columns: 2

    Item {
        id: data
        Layout.columnSpan: 2

        QRScanner {
            id: scanner
            scanType: QRScanner.PaymentDetails
            Component.onCompleted: scanner.start();
            onFinished: payment.targetAddress = scanResult
        }
        Payment {
            id: payment
        }
    }

    Flowee.Label {
        text: "to:"
    }
    Flowee.Label {
        text: payment.targetAddress
    }
    Flowee.Label {
        text: "amount:"
    }
    Flowee.Label {
        text: payment.paymentAmount
    }
    Flowee.Label {
        text: "comment:"
    }
    Flowee.Label {
        text: payment.userComment
    }
}
