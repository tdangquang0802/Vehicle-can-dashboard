import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: fuelBar
    width: 360
    height: 40

    // Giá trị nhiên liệu từ 0.0 (E) đến 1.0 (F)
    property real fuelValue: 0.8

    // Khung viền Neon
    Rectangle {
        id: bgFrame
        anchors.fill: parent
        color: "#0a0d1a"
        border.color: "#1a53ff"
        border.width: 2
        radius: 8

        // Chữ E (Empty)
        Text {
            id: labelE
            text: "E"
            color: fuelValue <= 0.15 ? "#ff3333" : "#888888"
            font.pixelSize: 16
            font.bold: true
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
        }

        // Thanh tiến trình (Fuel Level)
        Item {
            id: barContainer
            anchors.left: labelE.right
            anchors.right: labelF.left
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            height: 14

            Rectangle {
                anchors.fill: parent
                color: "#151928"
                radius: 4
            }

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, fuelBar.fuelValue))
                height: parent.height
                radius: 4
                color: fuelBar.fuelValue <= 0.15 ? "#ff3333" : "#1a53ff"

                Behavior on width { NumberAnimation { duration: 300 } }
            }

            // Vạch chia nhỏ 25% - 50% - 75%
            Row {
                anchors.fill: parent
                spacing: (parent.width - 6) / 4
                Repeater {
                    model: 3
                    Rectangle {
                        width: 2
                        height: parent.height
                        color: "#0a0d1a"
                    }
                }
            }
        }

        // Chữ F (Full)
        Text {
            id: labelF
            text: "F"
            color: fuelValue > 0.15 ? "#66b3ff" : "#888888"
            font.pixelSize: 16
            font.bold: true
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Glow {
        anchors.fill: bgFrame
        anchors.margins: -2
        source: bgFrame
        color: "#1a53ff"
        radius: 8
        samples: 12
        cached: true
    }
}