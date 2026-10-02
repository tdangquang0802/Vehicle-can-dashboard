import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: backgroundRoot
    anchors.fill: parent

    // 1. Rectangle gốc làm khung kích thước
    Rectangle {
        id: bgRect
        anchors.fill: parent
        color: "#0e2246" // Màu tâm
        visible: false  // Ẩn đi để RadialGradient vẽ đè lên
    }

    // 2. Hiệu ứng chuyển màu dạng hình tròn từ tâm tỏa ra
    RadialGradient {
        anchors.fill: parent
        source: bgRect

        // Tọa độ tâm tỏa sáng (Nằm chính giữa màn hình)
        horizontalOffset: 0
        verticalOffset: 0

        // Bán kính tỏa màu (Tùy chỉnh theo kích thước màn hình 1280x720)
        horizontalRadius: width / 1.6
        verticalRadius: height / 1.6

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0e2246" } // Màu tâm sáng nhẹ
            GradientStop { position: 0.7; color: "#061026" } // Màu trung gian
            GradientStop { position: 1.0; color: "#02050d" } // Màu mép ngoài tối thẫm
        }
    }
}