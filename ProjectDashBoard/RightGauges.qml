import QtQuick
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

Item {
    id: tempGauge
    width: 280
    height: 280

    // Giá trị nhiệt độ nhận từ 0.0 (Cold) đến 1.0 (Hot)
    property real tempValue: 0.5

    BackgroundGauges {
        anchors.fill: parent
        isLeft: true
        z: -1
    }
    // 1. Viền cong mở bên phải (Đối xứng ngược với đồng hồ RPM)
    Shape {
        id: tempBorder
        anchors.fill: parent
        antialiasing: true

        ShapePath {
            fillColor: "transparent"
            strokeColor: "#1a53ff"
            strokeWidth: 3
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: tempBorder.width / 2
                centerY: tempBorder.height / 2
                radiusX: (tempBorder.width / 2) - 4
                radiusY: (tempBorder.height / 2) - 4

                // Bắt đầu từ góc 50 độ (dưới phải) quét ngược 180 độ lên góc -130 độ (trên phải)
                startAngle: 55
                sweepAngle: -190
            }
            PathLine {
                x: tempBorder.width / 2
                y: tempBorder.height / 2
            }

            PathLine {
                x: (tempBorder.width / 2) + ((tempBorder.width / 2) - 4) * Math.cos(55 * Math.PI / 180)
                y: (tempBorder.height / 2) + ((tempBorder.height / 2) - 4) * Math.sin(55 * Math.PI / 180)
            }
        }
    }

    // 2. Hiệu ứng Glow đồng bộ
    Glow {
        anchors.fill: tempBorder
        anchors.margins: -2
        source: tempBorder
        color: "#1a53ff"
        radius: 10
        samples: 16
        cached: true
    }

    // 3. Các vạch chia (Ticks) & Chữ C - H
    Repeater {
        model: 9
        Item {
            width: parent.width
            height: parent.height
            // Xoay vạch chia ngược chiều: từ +140 deg về -40 deg
            rotation: 140 - (index * 22.5)

            Rectangle {
                width: index % 2 === 0 ? 4 : 2
                height: index % 2 === 0 ? 18 : 10
                radius: 3
                // Vạch gần mốc H sẽ có màu đỏ cảnh báo
                color: index >= 7 ? "#ff3333" : (index % 2 === 0 ? "#ffffff" : "#3399ff")
                anchors.horizontalCenter: parent.horizontalCenter
                y: 12
                antialiasing: true
            }

            Text {
                color: index === 8 ? "#ff3333" : "white"
                font.pixelSize: 18
                font.bold: true
                visible: index === 0 || index === 8
                anchors.horizontalCenter: parent.horizontalCenter
                y: 30
                // Đảo ngược góc quay để chữ luôn đứng thẳng
                rotation: -(140 - (index * 22.5))
            }
        }
    }

    Text {
        text: "TEMP"
        color: "#3399ff"
        font.pixelSize: 14
        anchors.verticalCenterOffset: 6
        anchors.horizontalCenterOffset: 35
        font.bold: true
        anchors.centerIn: parent   
    }

    // 4. Kim chỉ nhiệt độ
    Rectangle {
        width: 4
        height: 90
        color: tempValue > 0.8 ? "#ff3333" : "#ff4500"
        radius: 3
        anchors.bottom: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        transformOrigin: Item.Bottom

        // Quét góc quay từ 140 độ (C) giảm dần về -40 độ (H)
        rotation: 140 - (tempValue * 180)
        Behavior on rotation { SpringAnimation { spring: 3.0; damping: 0.3 } }
        antialiasing: true
    }

    // Tâm xoay
    Rectangle {
        width: 30
        height: 30
        radius: 15
        color: "#111"
        border.color: "#333"
        border.width: 3
        anchors.centerIn: parent
    }
}

