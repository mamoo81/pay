import QtQuick
import QtQuick.Controls as QQC2
// import QtQuick.Layouts
import "../Flowee" as Flowee
import Flowee.org.pay;
import QtQuick.Shapes // for shape-path

Item {
    id: root
    height: indicator.height + 3 + (circleShape.visible ? circleShape.height : 0)
    property QtObject account: null
    property bool hideWhenDone: false
    property int startPos: 1

    Component.onCompleted: {
        // remember the position we started the sync at.
        startPos = root.account.lastBlockSynched
    }

    // The 'progress' circle.
    Shape {
        id: circleShape

        property int goalHeight: Pay.expectedChainHeight
        visible: goalHeight - root.startPos > 500

        anchors.horizontalCenter: parent.horizontalCenter
        width: 200
        height: 100
        antialiasing: true
        smooth: true
        ShapePath {
            strokeColor: mainWindow.floweeBlue
            strokeWidth: 3
            fillColor: mainWindow.floweeGreen
            startX: 0; startY: 100

            PathAngleArc {
                id: progressCircle
                centerX: 100
                centerY: 100
                radiusX: 100; radiusY: 100
                startAngle: -180
                sweepAngle: {
                    let end = circleShape.goalHeight;
                    let startPos = root.startPos;
                    if (startPos == 0)
                        return 0;
                    let currentPos = root.account.lastBlockSynched;
                    let totalDistance = end - startPos;
                    if (totalDistance == 0)
                        return 180; // done
                    let ourProgress = currentPos - startPos;
                    return 180 * (ourProgress / totalDistance);
                }
                Behavior on sweepAngle {  NumberAnimation { duration: 50 } }
            }
            PathLine { x: 100; y: 100 }
            PathLine { x: 0; y: 100 }
        }
    }
    Flowee.Label {
        id: indicator
        width: parent.width
        anchors.top: circleShape.bottom
        anchors.topMargin: 3
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: {
            if (isLoading)
                return "";
            var account = portfolio.current;
            if (account === null)
                return "";
            if (account.needsPinToOpen && !account.isDecrypted)
                return qsTr("Offline");
            return account.timeBehind;
        }
        font.italic: true
    }
}
