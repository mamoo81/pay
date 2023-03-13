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

    Item { // data and non-visible stuff for this page
        /*
         * Components embedded in this file:
         *  Editors:
                destinationEditPage

              editors can use the property 'paymentDetail'

         *  ListItems:
                destinationFields

              list items have the property 'edit' to link to Editors
                 property Component edit: [something]
              list items can use the property 'paymentDetail'

         *  Other:
                paymentDetailSelector

         *  To open editors or list items, we use the pushToThePile() function.
         */
        Payment {
            id: payment
            account: portfolio.current
            fiatPrice: Fiat.price
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
                            text: qsTr("Add Destination")
                            subtext: qsTr("an address to send money to")
                            onClicked: {
                                var detail = payment.addExtraOutput();
                                thePile.pop();
                                pushToThePile(destinationEditPage,
                                    detail);
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
                property Component edit: destinationEditPage
                width: parent.width
                spacing: 6

                /* This page just shows the results, editing is done
                 * on its own page.
                 */
                Flowee.Label {
                    text: qsTr("Destination") + ":"
                }
                Flowee.LabelWithClipboard {
                    width: parent.width
                    font.italic: paymentDetail.address === ""
                    text: {
                        var s = paymentDetail.address
                        if (s === "")
                            return qsTr("unset", "indication of empty");
                        if (addressInfo.addressOk) {
                            let index = s.indexOf(":");
                            if (index >= 0)
                                s = s.substr(index + 1); // cut off the prefix
                        }
                        return s;
                    }
                    color: addressInfo.addressOk || paymentDetail.address === ""
                            ? palette.windowText : mainWindow.errorRed
                    menuText: qsTr("Copy Address")
                    clipboardText: paymentDetail.formattedTarget // the one WITH bitcoincash:
                }
                Flowee.AddressInfoWidget {
                    id: addressInfo
                    width: parent.width
                    addressType: Pay.identifyString(paymentDetail.address);
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

        /*
         * The different payment things work with a 'paymentDetail'
         * and since we push those into the global stack, they get
         * loaded and initialized first, only secondly we set the
         * property paymentDetail on them.
         *
         * This means we either get a load of errors dereferencing a null
         * object, or we need to alter a lot of code to account for that.
         *
         * This is the alternative solution: we add a layer of indirection
         * and set the property on the loader and the item in the loader
         * will simply find the paymentDetail present in the context in
         * which it has been loaded.
         */
        Component {
            id: loaderForPayments
            FocusScope {
                property alias paymentDetail: loader2.paymentDetail
                property Component sourceComponent: undefined
                function takeFocus() {
                    // this is also present in 'page', and called from thePile
                    forceActiveFocus();
                }
                Loader {
                    property QtObject paymentDetail: null
                    id: loader2
                    anchors.fill: parent
                    onLoaded: item.takeFocus();
                }
                function load() {
                    if (paymentDetail != null && typeof sourceComponent != "undefined") {
                        loader2.sourceComponent = sourceComponent
                    }
                }
                onPaymentDetailChanged: load();
                onSourceComponentChanged: load();
            }
        }

        Component {
            id: destinationEditPage
            Page {
                headerText: qsTr("Edit Destination")
                Flowee.Label {
                    id: destinationLabel
                    text: qsTr("Bitcoin Cash Address") + ":"
                }

                Flowee.MultilineTextField {
                    id: destination
                    anchors.top: destinationLabel.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    focus: true
                    property var addressType: Pay.identifyString(text);
                    text: paymentDetail.address
                    nextFocusTarget: priceInput
                    onTextChanged: {
                        paymentDetail.address = text
                        addressInfo.createInfo();
                    }
                    color: {
                        if (!activeFocus && text !== "" && !addressInfo.addressOk)
                            return mainWindow.errorRed
                        return palette.windowText
                    }
                }
                Flowee.LabelWithClipboard {
                    id: nativeLabel
                    width: parent.width
                    anchors.top: destination.bottom
                    anchors.topMargin: 10

                    visible: text !== ""
                    text: {
                        let string = paymentDetail.formattedTarget
                        let index = string.indexOf(":");
                        if (index >= 0) {
                            string = string.substr(index + 1); // cut off the prefix
                        }
                        return string;
                    }
                    clipboardText: paymentDetail.formattedTarget // the one WITH bitcoincash:
                    font.italic: true
                    menuText: qsTr("Copy Address")
                }
                Flowee.AddressInfoWidget {
                    id: addressInfo
                    anchors.top: nativeLabel.bottom
                    width: parent.width
                    addressType: destination.addressType
                }

                Rectangle {
                    color: mainWindow.errorRed
                    radius: 15
                    width: parent.width
                    height: warningColumn.height + 20
                    anchors.top: nativeLabel.bottom
                    // BTC address entered warning.
                    visible: (destination.addressType === Wallet.LegacySH
                                || destination.addressType === Wallet.LegacyPKH)
                            && paymentDetail.forceLegacyOk === false;

                    Column {
                        id: warningColumn
                        x: 10
                        y: 10
                        width: parent.width - 20
                        spacing: 10
                        Flowee.Label {
                            font.bold: true
                            font.pixelSize: warning.font.pixelSize * 1.2
                            text: qsTr("Warning")
                            color: "white"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Flowee.Label {
                            id: warning
                            width: parent.width
                            color: "white"
                            text: qsTr("This is a BTC address, which is an incompatible coin. Your funds could get lost and Flowee will have no way to recover them. Are you sure this is the right address?")
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }
                        Item {
                            width: parent.width
                            height: okButton.height
                            QQC2.Button {
                                id: okButton
                                anchors.right: parent.right
                                text: qsTr("I am certain")
                                onClicked: paymentDetail.forceLegacyOk = true
                            }
                        }
                    }
                }
                // TODO Max button.

                PriceInputWidget {
                    id: priceInput
                    width: parent.width
                    anchors.bottom: numericKeyboard.top
                    paymentBackend: paymentDetail
                    fiatFollowsSats: paymentDetail.fiatFollows
                    onFiatFollowsSatsChanged: paymentDetail.fiatFollows = fiatFollowsSats
                }
                NumericKeyboardWidget {
                    id: numericKeyboard
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 15
                    width: parent.width
                    enabled: !paymentDetail.maxSelected
                }
            }
        }
    }
    // check the comment at loaderForPayments to understand this one
    function pushToThePile(componentId, detail) {
        thePile.push(loaderForPayments,
            {"paymentDetail": detail,
             "sourceComponent": componentId }
        );
    }


    Flickable {
        id: contentArea
        anchors.fill: parent
        contentHeight: mainColumn.height + 10
        contentWidth: width
        clip: true
        Column {
            id: mainColumn
            y: 10
            width: parent.width

            Flowee.Label {
                text: qsTr("Create Payment")
                width: parent.width
                horizontalAlignment: Qt.AlignHCenter
            }
            Item { width: 1; height: 12 } // spacer

            Repeater {
                model: payment.details
                delegate: Item {
                    id: listItem
                    width: mainColumn.width
                    height: loader.height + 20 + (index <= 1 ? (dragInstructions.height + 20) : 0)

                    Loader {
                        id: loader
                        width: parent.width
                        height: status === Loader.Ready ? item.implicitHeight : 0
                        property QtObject paymentDetail: modelData
                        sourceComponent: {
                            if (modelData.type === Payment.PayToAddress)
                                return destinationFields
                            if (modelData.type === Payment.InputSelector)
                                return inputFields
                            return null; // should never happen
                        }
                    }

                    Item {
                        // an invisible item that is used to handle the drag events.
                        id: dragItem
                        width: parent.width
                        height: parent.height

                        property bool editStarted: false
                        onXChanged: {
                            /* When we move, we move the icons etc with us.
                             */
                            if (x < 0) {
                                leftArea.x = leftArea.width * -1 // out of screen
                                // moving left, opening the edit option
                                loader.x = x;
                                let a = x * -1;
                                editIcon.x = width - a + Math.max(0, a - editIcon.width);
                                if (!editStarted && x < -100) {
                                    editStarted = true;
                                    pushToThePile(loader.item.edit, modelData);
                                }
                            }
                            else {
                                editIcon.x = width;
                                // moving right, opening the trashcan option
                                let a = x;
                                let base = Math.min(40, a);
                                let rest = a - base;
                                deleteTimer.running = rest > 90;
                                if (rest < 90)
                                    deleteTimer.deleteTriggered = false
                                leftBackground.opacity = Math.max(rest - 40) / 100;
                                rest = Math.min(60, rest) / 4; // slower
                                leftArea.x = leftArea.width * -1 + base + rest;
                                loader.x = base + rest;
                            }
                        }

                        DragHandler {
                            id: dragHandler
                            // property bool editStarted: false;
                            yAxis.enabled: false
                            xAxis.enabled: true
                            xAxis.maximum: index > 0 ? 200 : 0 // swipe left
                            xAxis.minimum: -140 // swipe right
                            onActiveChanged: {
                                if (active) {
                                    parent.editStarted = false;
                                    return;
                                }
                                // take action on user releasing the drag.
                                if (deleteTimer.deleteTriggered) {
                                    deleteTimer.deleteTriggered = false;
                                    payment.remove(modelData);
                                }
                                parent.x = 0;
                            }
                        }
                    }

                    // delete-detail interface
                    Item {
                        id: leftArea
                        width: 300
                        height: loader.height
                        x: -width;
                        Rectangle {
                            id: leftBackground
                            opacity: 0
                            anchors.fill: parent
                            color: mainWindow.errorRed
                        }

                        Rectangle {
                            id: trashcan
                            color: "orange" // TODO replace this with an icon
                            width: 40
                            height: 40
                            y: 10
                            x: {
                                let newX = parent.width - width;
                                let moved = parent.x + parent.width;
                                let additional = Math.max(0, Math.min(moved - width, 8));
                                return newX - additional;
                            }
                        }

                        Timer {
                            /*
                              The intention of this timer is that the user can't just swipe hard
                              and suddenly lose their data.
                              We need to have the user actually see the red for half a second
                              before we delete.
                             */
                            id: deleteTimer
                            interval: 400
                            property bool deleteTriggered: false
                            onTriggered: deleteTriggered = true
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

                    // UX help
                    Row {
                        id: row
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 26
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6
                        visible: index <= 1
                        property bool dragToEdit: index === 0
                        Repeater {
                            model: 4
                            delegate: Flowee.ArrowPoint { y: 3; color: palette.highlight
                                rotation: row.dragToEdit ? 180 : 0;
                            }
                        }
                        Flowee.Label {
                            id: dragInstructions
                            text: index === 0 ? qsTr("Drag to Edit") : qsTr("Drag to Delete");
                            font.italic: true
                            color: palette.highlight
                        }
                        Repeater {
                            model: 4
                            delegate: Flowee.ArrowPoint { y: 3; color: palette.highlight
                                rotation: row.dragToEdit ? 180 : 0;
                            }
                        }
                    }

                    VisualSeparator {
                        anchors.bottom: parent.bottom
                    }

                    Behavior on x { NumberAnimation { duration: 100 } }
                }
            }

            TextButton {
                text: qsTr("Add Destination")
                showPageIcon: true
                onClicked: pushToThePile(destinationEditPage, payment.addExtraOutput());
            }
/*
            TextButton {
                text: qsTr("Add Detail...")
                showPageIcon: true
                onClicked: thePile.push(paymentDetailSelector);
            }
*/

        }

        // Add "Next" button here.
        // on 'next' prepare and go to 'swipe to send' screen.
        //    we likely want to have transaction details shown on that screen.
    }

}
