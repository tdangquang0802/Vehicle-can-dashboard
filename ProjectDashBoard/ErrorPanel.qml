import QtQuick
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

Item {
    id: errorPanel
    width: 500
    height: 50

    // Trạng thái lỗi giả lập (có thể kết nối với C++ sau)
    property bool engineError: true
    property bool batteryError: false
    property bool oilError: true
    property bool absError: false

    // Khung nền mờ và viền cảnh báo
    Rectangle {
        id: bgRect
        anchors.fill: parent
        color: "#1a0505" // Đen ám đỏ đặc trưng cho cảnh báo lỗi
        border.color: "#ff3300" // Viền đỏ cam cảnh báo
        border.width: 1.5
        radius: 6
        opacity: 0.85
    }

    // Hiệu ứng phát sáng nhẹ cho khung
    Glow {
        anchors.fill: bgRect
        source: bgRect
        color: "#ff3300"
        radius: 8
        samples: 16
        opacity: 0.4
        cached: true
    }

    // Khu vực chứa các icon lỗi sắp xếp ngang
    Row {
        anchors.centerIn: parent
        spacing: 50

        // 1. Icon Check Engine
        ErrorIconItem {
            iconText: "ENG"
            active: errorPanel.engineError
            activeColor: "#ff3300"
        }

        // 2. Icon Bình Ắc-quy (Battery)
        ErrorIconItem {
            iconText: "BAT"
            active: errorPanel.batteryError
            activeColor: "#ff9900"
        }

        // 3. Icon Dầu nhớt (Oil Pressure)
        ErrorIconItem {
            iconText: "OIL"
            active: errorPanel.oilError
            activeColor: "#ff3300"
        }

        // 4. Icon Hệ thống phanh (ABS)
        ErrorIconItem {
            iconText: "ABS"
            active: errorPanel.absError
            activeColor: "#ff3300"
        }
    }

    // Component con cho từng icon bên trong khung
    component ErrorIconItem: Item {
        property string iconText: "ERR"
        property bool active: false
        property color activeColor: "#ff3300"

        width: 60
        height: 35

        Rectangle {
            anchors.fill: parent
            color: active ? activeColor : "#0d0202"
            border.color: active ? "#ffffff" : "#441111"
            border.width: 1
            radius: 4

            // Hiệu ứng nhấp nháy khi lỗi ở mức độ nghiêm trọng
            SequentialAnimation on opacity {
                running: active
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 400 }
                NumberAnimation { to: 1.0; duration: 400 }
            }

            Text {
                anchors.centerIn: parent
                text: parent.parent.iconText
                color: active ? "#ffffff" : "#553333"
                font.pixelSize: 13
                font.bold: true
                font.family: "Consolas"
            }
        }
    }
}