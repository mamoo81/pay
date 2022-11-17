import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 2.15
import "../Flowee" as Flowee

Flickable {
    id: accountHistory
    property string icon: "qrc:/bla" // TODO make icon and show on tab

    contentHeight: column.height + 20
    clip: true
    Rectangle {
        // background color "base", to indicate these are widgets here.
        anchors.fill: parent
        color: mainWindow.palette.base
        ColumnLayout {
            id: column
            spacing: 20
            width: parent.width - 20
            x: 10
            y: 10

            Item {
                id: smallAccountSelector
                width: parent.width
                height: walletSelector.height
                z: 10

                property bool open: false

                visible: {
                    // if there is only an initial, not user-owned wallet, there is nothing to show here.
                    return portfolio.current.isUserOwned
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
                                    text: "today";
                                    font.pointSize: mainWindow.font.pointSize * 0.8
                                    font.bold: false
                                }
                                Flowee.Label {
                                    anchors.right: parent.right
                                    anchors.top: accountName.top
                                    text: "€ 123.00";
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
            Flowee.Label {
                id: balanceLabel
                text: "€ 100";
            }

            GridLayout {
                width: parent.width
                columns: 3

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 60
                    height: 60
                    radius: 30
                    color: "#00000000"
                    border.color: "yellow"
                    border.width: 1
                }
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 60
                    height: 60
                    radius: 30
                    color: "#00000000"
                    border.color: "yellow"
                    border.width: 1
                }
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 60
                    height: 60
                    radius: 30
                    color: "#00000000"
                    border.color: "yellow"
                    border.width: 1
                }
                Flowee.Label {
                    text: "Send Money"
                    horizontalAlignment: Qt.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                }
                Flowee.Label {
                    text: "Scheduled"
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                }
                Flowee.Label {
                    text: "Receive"
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                }
            }

            Item { width: 1; height: 10 } // spacer

            Flowee.Label {
                text: "Earlier this month"
            }
            Item {
                width: parent.width
                height: 100
                Rectangle {
                    width: parent.width - 30
                    x: 15
                    radius: 10
                    height: 100
                    color: mainWindow.palette.window
                }
            }
        }
    }
}
