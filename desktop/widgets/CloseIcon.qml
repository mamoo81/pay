import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import Flowee.org.pay 1.0

Item {
    id: root
    width: 30
    height: 30

    signal clicked;
    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked();
    }

    Rectangle {
        color: "#bbbbbb"
        width: 30
        height: 3
        radius: 3
        rotation: 45
        anchors.centerIn: parent
    }
    Rectangle {
        color: "#bbbbbb"
        width: 30
        height: 3
        radius: 3
        rotation: -45
        anchors.centerIn: parent
    }
}
