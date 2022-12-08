import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;

Item {
    id: smallAccountSelector
    height: walletSelector.height
    property bool open: false

    visible: {
        // if there is only an initial, not user-owned wallet, there is nothing to show here.
        if (!portfolio.current.isUserOwned)
            return false;
        if (portfolio.accounts.length === 1)
            return false;
        return true;
    }

    Rectangle {
        id: walletSelector
        height: currentWalletName.height + 14
        width: parent.width + 5
        color: "#00000000"
        border.color: mainWindow.palette.button
        border.width: hasMultipleWallets ? 0.7 : 0
        x: -5

        property bool hasMultipleWallets: portfolio.accounts.length > 1

        Flowee.Label {
            x: 5
            id: currentWalletName
            text: portfolio.current.name
            anchors.verticalCenter: parent.verticalCenter
        }
        Flowee.ArrowPoint {
            id: arrowPoint
            visible: parent.hasMultipleWallets
            color: mainWindow.palette.text
            anchors.right: parent.right
            anchors.rightMargin: 10
            rotation: 90
            anchors.verticalCenter: parent.verticalCenter
        }
        MouseArea {
            anchors.fill: parent
            enabled: parent.hasMultipleWallets
            onClicked: {
                smallAccountSelector.open = !smallAccountSelector.open
                // move focus so the 'back' button will close it.
                extraWallets.forceActiveFocus();
            }
        }
    }

    Rectangle {
        id: extraWallets
        width: parent.width
        y: walletSelector.height
        height: parent.open ? columnLayout.height + 20 : 0
        color: mainWindow.palette.base
        Behavior on height {
            NumberAnimation {
                duration: 200
            }
        }

        clip: true
        visible: height > 0
        focus: true
        ColumnLayout {
            y: 10
            id: columnLayout
            width: parent.width
            Item {
                // horizontal divider.
                width: parent.width
                height: 1
                Rectangle {
                    height: 1
                    width: parent.width / 10 * 7
                    x: (parent.width - width) / 2 // center in column
                    color: mainWindow.palette.highlight
                }
            }

            Repeater { // portfolio holds all our accounts
                width: parent.width
                model: portfolio.accounts
                delegate: Item {
                    width: extraWallets.width
                    height: accountName.height * 2 + 12
                    Rectangle {
                        color: mainWindow.palette.button
                        anchors.fill: parent
                        visible: modelData === portfolio.current
                    }
                    Flowee.Label {
                        id: accountName
                        y: 6
                        text: modelData.name
                    }
                    Flowee.Label {
                        id: lastActive
                        anchors.top: accountName.bottom
                        text: qsTr("last active") + ": "
                        font.pointSize: mainWindow.font.pointSize * 0.8
                        font.bold: false
                    }
                    Flowee.Label {
                        anchors.top: lastActive.top
                        anchors.left: lastActive.right
                        text: Pay.formatDate(modelData.lastMinedTransaction)
                        font.pointSize: mainWindow.font.pointSize * 0.8
                        font.bold: false
                    }
                    Flowee.BitcoinAmountLabel {
                        anchors.right: parent.right
                        anchors.top: accountName.top
                        showFiat: true
                        value: modelData.balanceConfirmed + modelData.balanceUnconfirmed
                        colorize: false
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            portfolio.current = modelData
                            smallAccountSelector.open = false
                        }
                    }
                }
            }
            Item {
                // horizontal divider.
                width: parent.width
                height: 1
                Rectangle {
                    height: 1
                    width: parent.width / 10 * 7
                    x: (parent.width - width) / 2 // center in column
                    color: mainWindow.palette.highlight
                }
            }
        }

        Keys.onPressed: (event)=> {
                            if (event.key === Qt.Key_Back) {
                                event.accepted = true;
                                smallAccountSelector.open = false
                            }
                        }
    }
}
