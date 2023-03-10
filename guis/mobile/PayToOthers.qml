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
import QtQuick.Controls as QQC2
import "../Flowee" as Flowee
import Flowee.org.pay;

Page {
    id: root
    headerText: qsTr("Create Payment")

    Item { // data
        QRScanner {
            id: scanner
            scanType: QRScanner.PaymentDetails
            /*onFinished: {
                var rc = scanResult
                if (rc === "") { // scanning failed
                    thePile.pop();
                }
                else {
                    payment.targetAddress = rc
                    priceBch.forceActiveFocus();
                }
            } */
        }
        Payment {
            id: payment
            account: portfolio.current
            fiatPrice: Fiat.price
        }

        AccountSelector {
            id: accountSelector
            width: root.width
            x: -10 // to correct the indent added in the fullPage
            y: (root.height - height) / 2
            onSelectedAccountChanged: payment.account = selectedAccount
            selectedAccount: payment.account
        }

        // Extra page to create new details.
        Component {
            id: paymentDetailSelector
            Page {
                headerText: qsTr("Add Payment Detail", "page title")
                Flickable {
                    anchors.fill: parent
                    contentHeight: col.height
                    Column {
                        id: col
                        width: parent.width
                        TextButton {
                            text: "Add Destination"
                            onClicked: {
                                payment.addExtraOutput();
                                thePile.pop();
                            }
                        }
                    }
                }
            }
        }

        // listitem of PaymentDetail showing payment-destination
        Component {
            id: destinationFields
            Column {
                property QtObject paymentDetail: parent.paymentDetail
                property Component edit: destinationEditPage
                width: parent.width
                spacing: 6

                /* This page just shows the results, editing is done
                 * on its own page.
                 */
                Flowee.Label {
                    text: qsTr("Addressee") + ":"
                }
                Flowee.LabelWithClipboard {
                    width: parent.width
                    text: paymentDetail.address
                    menuText: qsTr("Copy Address")
                }
                Flowee.Label {
                    text: qsTr("Amount")+ ":"
                }
                Flowee.BitcoinAmountLabel {
                    value: paymentDetail.paymentAmount
                    colorize: false
                }
            }
        }

        Component {
            id: destinationEditPage
            Page {
                headerText: qsTr("Edit Destination")
            }
        }
    }

    Flickable {
        id: contentArea
        anchors.fill: parent
        contentHeight: mainColumn.height
        contentWidth: width
        clip: true
        Column {
            id: mainColumn
            width: parent.width
            spacing: 10

            Repeater {
                model: payment.details
                delegate: Item {
                    id: listItem
                    width: mainColumn.width
                    height: loader.height + 6

                    Loader {
                        id: loader
                        width: parent.width
                        height: status === Loader.Ready ? item.implicitHeight : 0
                        sourceComponent: {
                            if (modelData.type === Payment.PayToAddress)
                                return destinationFields
                            if (modelData.type === Payment.InputSelector)
                                return inputFields
                            return null; // should never happen
                        }
                        onLoaded: item.paymentDetail = modelData
                    }

                    // delete-detail interface
                    Rectangle {
                        id: leftBackground
                        property bool aboutToDelete: parent.x > 90
                        anchors.right: loader.left
                        color: aboutToDelete ? "red" : "#00000000" // TODO make change happen earlier.
                        width: 300
                        height: parent.height
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Rectangle {
                            id: trashcan
                            color: "orange" // TODO replace this with an icon
                            width: 40
                            height: 40
                            y: 10
                            x: {
                                let newX = parent.width - width;
                                let additional = Math.max(0, Math.min(listItem.x - width, 16))
                                return newX - additional;
                            }
                        }
                    }

                    Rectangle {
                        id: editIcon
                        color: "blue"
                        width: 40
                        height: 40
                        y: 10
                        x: {
                            let additional = Math.max(0, Math.min(listItem.x * -1 - width, 16))
                            return parent.width + additional;
                        }
                    }

                    onXChanged: {
                        if (x < -75 && dragHandler.editStarted === false) {
                            dragHandler.editStarted = true;
                            dragHandler.enabled = false; // stop dragging
                            thePile.push(loader.item.edit)
                        }
                    }

                    DragHandler {
                        id: dragHandler
                        property bool editStarted: false;
                        yAxis.enabled: false
                        xAxis.enabled: true
                        xAxis.minimum: -140 // swipe right
                        xAxis.maximum: 200 // swipe left
                        onActiveChanged: {
                            if (active) {
                                editStarted = false;
                                return;
                            }
                            if (editStarted) {
                                enabled = true;
                                parent.x= 0;
                                return;
                            }
                            // take action on user releasing the drag.
                            if (leftBackground.aboutToDelete)
                                payment.remove(modelData);
                            else // reset pos
                                parent.x = 0
                        }
                    }
                    Behavior on x { NumberAnimation { duration: 100 } }
                }
            }

            TextButton {
                text: qsTr("Add Detail...")
                showPageIcon: true
                onClicked: thePile.push(paymentDetailSelector);
            }

        }

        // Add 'Next' button in header
        // Add "Next" button here.
        // on 'next' prepare and go to 'swipe to send' screen.
        //    we likely want to have transaction details shown on that screen.
    }

}
