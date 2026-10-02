import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import ProjectDashBoard

ApplicationWindow {
    id: window
    width: 1280
    height: 720
    visible: true
    title: qsTr("Dash Board")
    color: "#020205"

    // Khai báo biến lưu trữ dữ liệu thật từ C++
    property double currentSpeed: 0
    property real currentRPM: 0

    // Các phần giả lập khác (nhiệt độ, xăng, xi nhan) giữ nguyên hoặc tùy chỉnh sau
    property real currentTemp: 50
    property real currentFuel: 80
    property bool leftSignalActive: false
    property bool rightSignalActive: false


    // Khởi tạo kết nối Backend C++ (Carbackend)
    Connections {
        target: carbackend // Tên này phải khớp với tên đã setContextProperty bên main.cpp

        function onSpeedChanged(speed) {
            window.currentSpeed = speed
        }

        function onRpmChanged(rpm) {
            window.currentRPM = rpm
        }
    }

    Text {
        text: "Debug Speed: " + window.currentSpeed + " | RPM: " + window.currentRPM
        color: "yellow"
        font.pixelSize: 18
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20
        z: 999 // Đảm bảo nổi lên trên cùng
    }

    Item {
        anchors.fill: parent
        focus: true // Nhận diện phím gõ (xi nhan)

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_A) {
                window.leftSignalActive = !window.leftSignalActive
                if (window.leftSignalActive) {
                    window.rightSignalActive = false
                }
            }
            else if (event.key === Qt.Key_D) {
                window.rightSignalActive = !window.rightSignalActive
                if (window.rightSignalActive) {
                    window.leftSignalActive = false
                }
            }
        }
    }

    Item {
        id: dashboardRoot
        width: 1280
        height: 720
        anchors.centerIn: parent
        scale: Math.min(parent.width / width, parent.height / height)

        BackgroundDB {
            anchors.fill: parent
        }

        FuelBar {
            anchors.horizontalCenter: speedometer.horizontalCenter
            anchors.top: speedometer.bottom
            anchors.topMargin: 20
            fuelValue: window.currentFuel / 100
        }

        RightGauges {
            anchors.right: parent.right
            anchors.rightMargin: 80
            anchors.verticalCenter: parent.verticalCenter
            tempValue: window.currentTemp / 100
        }

        // Đồng hồ Vòng tua (RPM) nhận dữ liệu thật từ currentRPM
        RPM {
            id: tachometer
            rpmValue: window.currentRPM
            anchors.right: speedometer.left
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
        }

        ErrorPanel {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 30
        }

        // Xi nhan Trái
        TurnSignal {
            id: leftSignal
            isLeft: true
            scale: 0.6
            anchors.verticalCenter: speedometer.verticalCenter
            anchors.verticalCenterOffset: 70
            anchors.right: speedometer.left
            anchors.rightMargin: -80
            active: window.leftSignalActive
        }

        // Xi nhan Phải
        TurnSignal {
            id: rightSignal
            isLeft: false
            scale: 0.6
            anchors.verticalCenter: speedometer.verticalCenter
            anchors.verticalCenterOffset: 70
            anchors.leftMargin: -80
            anchors.left: speedometer.right
            active: window.rightSignalActive
        }

        // Đồng hồ Tốc độ (Speedometer) chính giữa
        Item {
            id: speedometer
            width: 500
            height: 500
            anchors.centerIn: parent

            Image {
                id: meterBg
                anchors.fill: parent
                source: "assets/images/background_meter.jpg"
                fillMode: Image.Stretch
                visible: false
            }

            Rectangle {
                id: circularMask
                anchors.fill: parent
                radius: width / 2
                color: "black"
                visible: false
            }

            OpacityMask {
                anchors.fill: parent
                source: meterBg
                maskSource: circularMask
            }

            Rectangle {
                id: speedometerBorder
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: "#1a53ff"
                border.width: 3
                opacity: 0.5
                antialiasing: true
            }

            Repeater {
                model: 25

                Item {
                    width: speedometer.width
                    height: speedometer.height
                    rotation: -120 + (index * 10)

                    Rectangle {
                        width: index % 2 === 0 ? 4 : 2
                        height: index % 2 === 0 ? 20 : 12
                        radius: 3
                        color: index % 2 === 0 ? "#ffffff" : "#3399ff"
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 10
                        antialiasing: true
                    }

                    Text {
                        text: index * 10
                        color: "white"
                        font.pixelSize: 20
                        font.bold: true
                        visible: index % 2 === 0
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 40
                        rotation: -(-120 + (index * 10))
                    }
                }
            }

            Glow {
                anchors.fill: speedometerBorder
                anchors.margins: -2
                source: speedometerBorder
                color: "#1a53ff"
                radius: 10
                samples: 16
                cached: true
            }

            Text {
                text: "KM/H"
                color: "#66b3ff"
                font.pixelSize: 20
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -30
            }

            // Kim đồng hồ tốc độ chạy theo currentSpeed thật
            Rectangle {
                id: needle
                width: 6
                height: 200
                color: "#ff4500"
                radius: 4
                antialiasing: true

                anchors.bottom: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                transformOrigin: Item.Bottom

                Rectangle {
                    id: mainNeedle
                    width: 5
                    height: 195
                    color: "#ff4500"
                    radius: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    antialiasing: true
                }

                Rectangle {
                    width: 2
                    height: 190
                    color: "#ffccaa"
                    radius: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    antialiasing: true
                }

                // Góc quay gắn trực tiếp với biến tốc độ thực tế
                rotation: -120 + window.currentSpeed

                Behavior on rotation {
                    SmoothedAnimation {
                        duration: 150
                        reversingMode: SmoothedAnimation.Eased
                    }
                }
            }

            Rectangle {
                id: centerDotNeedle
                width: 24
                height: 24
                radius: width / 3
                color: "#111"
                border.color: "#333"
                border.width: 3
                anchors.centerIn: parent

                Rectangle {
                    width: 10
                    height: 10
                    radius: 3
                    color: "#ff4500"
                    anchors.centerIn: parent
                }
            }
        }

        // Hiển thị con số tốc độ dạng số (Digital Speed) chính xác từ phần cứng gửi lên
        Rectangle {
            width: 100
            height: 40
            color: "#002244"
            radius: 5
            anchors.horizontalCenter: parent.horizontalCenter
            y: speedometer.height / 2 + 200

            Text {
                text: Math.round(window.currentSpeed)
                color: "#00d4ff"
                font.pixelSize: 26
                font.bold: true
                anchors.centerIn: parent
            }
        }
        Text {
            text: "Debug Speed: " + window.currentSpeed + " | RPM: " + window.currentRPM
            color: "yellow"
            font.pixelSize: 18
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 20
            z: 999 // Đảm bảo nổi lên trên cùng
        }
    }
}