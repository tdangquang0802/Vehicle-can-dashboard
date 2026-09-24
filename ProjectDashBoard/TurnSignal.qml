import QtQuick
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

Item {
    id: root
    width: 295
    height: 565

    property bool isLeft: true
    property bool active: false

    // Khai báo chuẩn dải màu Cam Neon
    property color neonOrange: "#ff5500"    // Cam neon rực rỡ
    property color darkOrangeBg: "#1a0800"  // Đen ám cam (màu nền khi tắt)

    // Timer nhấp nháy
    property bool _flashState: false
    Timer {
        interval: 400
        running: root.active
        repeat: true
        onTriggered: root._flashState = !root._flashState
        onRunningChanged: if (!running) root._flashState = false
    }

    // Lật ngược hình
    transform: Scale {
        origin.x: root.width / 2
        origin.y: root.height / 2
        xScale: root.isLeft ? -1 : 1
    }

    Shape {
        id: signalShape
        anchors.fill: parent
        antialiasing: true

        ShapePath {
            // NỀN: Sáng rực màu cam khi có tín hiệu nhấp nháy, lúc tắt thì quay về nền tối
            fillColor: (root.active && root._flashState) ? root.neonOrange : root.darkOrangeBg

            // VIỀN: Luôn giữ viền màu cam neon
            strokeColor: root.neonOrange
            strokeWidth: 3 // Tăng độ dày viền lên 3 để thấy màu cam rõ hơn
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            // Đường vẽ tay SVG của bạn
            PathSvg {
                path: "M57.5795 0.313721C127.863 187.042 114.278 285.104 0.579498 448.814L57.5795 563.314L294.079 293.814L57.5795 0.313721Z"
            }
        }
    }

    // HIỆU ỨNG GLOW PHÁT SÁNG NỀN VÀ VIỀN
    Glow {
        anchors.fill: signalShape
        anchors.margins: -10
        anchors.leftMargin: 0
        anchors.rightMargin: -20
        anchors.topMargin: 0

        anchors.bottomMargin: -20 // Mở rộng bán kính tỏa sáng ra ngoài
        source: signalShape
        color: root.neonOrange

        // Khi chớp sáng: Quầng sáng to (radius 25) và rõ rệt (opacity 1.0)
        // Khi tắt: Chỉ còn viền cam phát sáng lờ mờ (radius 8, opacity 0.2)
        radius: (root.active && root._flashState) ? 14 : 7
        opacity: (root.active && root._flashState) ? 0.8 : 0.2
        cached: true
    }
}