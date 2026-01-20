import QtQuick
import QtQuick.Controls
Item {
    id: root
    property real progress: 0
    property real radiusprogress: 0
    property string colorprogress: "green"
    //signal progressClick (real value)
    Rectangle {
        id: progressBarBackground
        anchors.fill: parent
        color: "gray"
        radius: radiusprogress
        Rectangle {
            id: progressBarFill
            width: parent.width * progress
            height: parent.height
            color: root.colorprogress
            radius: radiusprogress
            Behavior on width {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.Linear
                }
            }
        }
    }
}
