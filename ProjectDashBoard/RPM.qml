import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
Item {
    width: 280
    height: 280

    // Thuộc tính mở để nhận dữ liệu từ main.qml
    property real rpmValue: 0

    BackgroundGauges {
        anchors.fill: parent
        isLeft: false
        z: -1
    }

    Shape {
        id: tachometerBorder
        anchors.fill: parent
        antialiasing: true

        ShapePath {
            fillColor: "transparent"
            strokeColor: "#1a53ff" // Màu viền
            strokeWidth: 3
            capStyle: ShapePath.RoundCap // Bo tròn 2 đầu nét cắt cho mềm mại
            joinStyle: ShapePath.RoundJoin // Hoặc ShapePath.BevelJoin

            PathAngleArc {
                centerX: tachometerBorder.width / 2
                centerY: tachometerBorder.height / 2
                radiusX: (tachometerBorder.width / 2) - 4
                radiusY: (tachometerBorder.height / 2) - 4

                startAngle: 125
                sweepAngle: 190
            }

            PathLine {
                x: tachometerBorder.width / 2
                y: tachometerBorder.height / 2
            }
            // 2. Kéo tiếp từ TÂM VỀ ĐỈNH BAN ĐẦU (125 độ)
            PathLine {
                x: (tachometerBorder.width / 2) + ((tachometerBorder.width / 2) - 4) * Math.cos(125 * Math.PI / 180)
                y: (tachometerBorder.height / 2) + ((tachometerBorder.height / 2) - 4) * Math.sin(125 * Math.PI / 180)
            }
        }
    }

    Glow {
        anchors.fill: tachometerBorder
        source: tachometerBorder
        color: "#1a53ff"
        radius: 10
        samples: 16
    }

    Repeater {
        model: 17
        Item {
            width: parent.width
            height: parent.height
            rotation: -140 + (index * 11.25)

            Rectangle {
                width: index % 2 === 0 ? 4 : 2
                height: index % 2 === 0 ? 18 : 10
                radius: 3
                color: index >= 12 ? "#ff3333" : (index % 2 === 0 ? "#ffffff" : "#3399ff")
                anchors.horizontalCenter: parent.horizontalCenter
                y: 12
                antialiasing: true
            }

            Text {
                text: index / 2
                color: index >= 12 ? "#ff3333" : "white"
                font.pixelSize: 16
                font.bold: true
                visible: index % 2 === 0
                anchors.horizontalCenter: parent.horizontalCenter
                y: 32
                rotation: -(-140 + (index * 11.25))
            }
        }
    }

    Text {
        text: "x1000 r/m"
        color: "#3399ff"
        font.pixelSize: 14
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 30
    }

    // Kim đồng hồ sử dụng biến rpmValue
    Rectangle {
        width: 4
        height: 90
        color: "#ff4500"
        radius: 3
        anchors.bottom: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        transformOrigin: Item.Bottom

        // Khai báo khoảng góc cho mặt đồng hồ RPM
        readonly property real minAngle: -140  // Góc ứng với vạch 0
        readonly property real maxAngle: 50    // Góc ứng với vạch 8
        readonly property real maxRPM: 8000    // RPM tối đa trên mặt đồng hồ

        // Công thức tính góc quay theo tỷ lệ RPM
        rotation: minAngle + (Math.min(Math.max(rpmValue, 0), maxRPM) / maxRPM) * (maxAngle - minAngle)

        Behavior on rotation {
            SpringAnimation { spring: 3.0; damping: 0.3 }
        }
        antialiasing: true
    }

    Rectangle {
        width: 30
        height: 30; radius: 15
        color: "#111"; border.color: "#333"; border.width: 3
        anchors.centerIn: parent
    }
}