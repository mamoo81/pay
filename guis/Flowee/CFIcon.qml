import QtQuick 2.11
import QtQuick.Controls 2.11 as QQC2

Image {
    id: fusedIcon
    visible: fusedCount > 0
    source: "qrc:/cf.svg"
    width: 24
    height: 24
    QQC2.ToolTip {
        delay: 600
        text: qsTr("Coin has been fused for increased anonymity")
        visible: mouseArea.inside
    }
    MouseArea {
        id: mouseArea
        property bool inside: false
        hoverEnabled: true
        anchors.fill: parent
        anchors.margins: -5
        onEntered: inside = true
        onExited: inside = false
    }
}
