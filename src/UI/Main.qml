import QtQuick
import QtQuick.Controls
import RealTimeLib 1.0
import QtQuick.Layouts
import QtQuick.Effects

Window {
    id: root
    visible: true
    width: 900
    height: 500
    ////////BACK GROUND////////
    Image {
        id: backgroundImage
        anchors.fill: parent
        source: "qrc:/image/assets/backgroundTest.jpg"
    }
    ////////////////////////////////
    /////PROPERTY//////////////////////
    property real speed: cluster.speed
    property real rpm: cluster.rpm
    property bool num1: false
    property bool num2: false
    property bool num3: false
    property bool num4: false
    property bool num5: false
    property bool num6: false
    property bool num7: false
    property real progressFuel: 0.6
    property real progressTemp: 0.5
    property int stateIndex: 0
    property var states: ["normal","can", "chart"]
    function updateState() {
        root.mode = root.states[root.stateIndex]
    }
    property string mode: "normal"
    //////////////////////////////////////
    ////////// CHẠY THỜI GIAN THỰC //////////
    RealTime
    {
        id: realTime
        Component.onCompleted: {
            startTimer()
        }
    }
    /////////////////////////////////////////////////////////
    //// HIỂN THỊ ĐỒNG HỒ TỐC ĐỘ
    Rectangle
    {
        id: rectSpeed
        width: root.width/3
        height: rectSpeed.width
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 20
        color: "transparent"
        DongHoTocDo
        {
            id: speedGause
            width: parent.width
            height: width
            anchors.verticalCenter: parent.verticalCenter
            speed: root.speed
            iconSource: "qrc:/image/assets/purple.png"
        }
    }
    ///////////////////////////////////////////////////////////////
    //// HIỂN THỊ VÙNG MÀN HÌNH ////
    ////MÀN HÌNH CHUYỂN ĐỔI GIỮA 3 STATE//////////////
    Rectangle
    {
        id:rectDisplay
        height: rectDisplay.width
        anchors.left: rectSpeed.right
        anchors.right: rectRpm.left
        anchors.verticalCenter: parent.verticalCenter
        color: "transparent"
        //property string mode: "normal"

        Loader
                {
                    id: normalState
                    anchors.fill: parent
                    source: "CarBackView.qml"
                    active: mode === "normal"
                    // onLoaded:
                    // {
                    //     normalState.item.speed = Qt.biding(function(){
                    //     root.s
                    // }
                }
                // Loader
                // {
                //     id: chartState
                //     anchors.fill: parent
                //     source: "Chart.qml"
                //     active: mode === "chart"
                // }
                Loader
                {
                    id: canState
                    anchors.fill: parent
                    source: "CAN.qml"
                    active: mode === "can"
                }

    }
    ///////////////////////////////////////////////////////////
    ///////// HIỂN THỊ ĐỒNG TỐC TỐC ĐỘ ĐỘNG CƠ /////////////
    Rectangle
    {
        id: rectRpm
        width: root.width/3
        height: rectRpm.width
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 20
        color: "transparent"
        DongHoRPM
        {
            id: rpmGause
            width: parent.width
            height: width
            anchors.verticalCenter: parent.verticalCenter
            rpm: root.rpm
            iconSource: "qrc:/image/assets/purple.png"
        }
    }
    //////////////////////////////////////////////////
    ///////////// HIỂN THỊ CÁC BIỂU TƯỢNG CẢNH BÁO /////////////
    Rectangle
    {
        id: rectDiag
        anchors.bottom: rectSpeed.top
        anchors.top: parent.top
        width: parent.width/3
        x: (parent.width-width)/2
        color: "transparent"
        RowLayout
        {
            anchors.fill: rectDiag
            spacing: 20
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Image {
                    id: battery
                    source: "qrc:/image/assets/battery1.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num1
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Image {
                    id: oil
                    source: "qrc:/image/assets/oilpressure1.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num2
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Image {
                    id: abs
                    source: "qrc:/image/assets/abs1.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num3
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Image {
                    id: airbag
                    source: "qrc:/image/assets/airbag1.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num4
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Image {
                    id: brake
                    source: "qrc:/image/assets/brake-system-warning.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num5
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight:40
                Image {
                    id: engine
                    source: "qrc:/image/assets/engine.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num6
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Image {
                    id: temperature
                    source: "qrc:/image/assets/temp.png"
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    visible: num7
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////
    ////////// HIỂN THỊ MỨC XĂNG////////////////////////////////
    Rectangle
    {
        id: rectFuel
        anchors.top: rectSpeed.bottom
        anchors.bottom: parent.bottom
        anchors.right: rectDiag.left
        anchors.left: parent.left
        anchors.leftMargin: 20
        color: "transparent"
        MyProgressBar
        {
            id: fuelBar
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            height: 32
            width: parent.width
            radiusprogress: height/2
            colorprogress: progressFuel < 0.2 ? "red" : "green"
            progress: progressFuel
        }
        Image {
            id: fuelImage
            source: "qrc:/image/assets/local_gas_station_24dp_1F1F1F_FILL0_wght400_GRAD0_opsz24.png"
            width: 32
            height: width
            anchors.centerIn: fuelBar
            fillMode: Image.PreserveAspectFit
        }
    }
    ////////////////////////////////////////////////////////
    /////////HIỂN THỊ NHIỆT ĐỘ ĐỘNG CƠ//////////////////
    Rectangle
    {
        id: rectTemp
        anchors.top: rectRpm.bottom
        anchors.bottom: parent.bottom
        anchors.left: rectDiag.right
        anchors.right: parent.right
        anchors.rightMargin: 20
        color: "transparent"

        MyProgressBar
        {
            id: tempBar
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 32
            width: parent.width
            radiusprogress: height/2
            colorprogress: root.coolant > 100 ? "red" : "green"
            progress: root.progressTemp
        }
        Image {
            id: tempImage
            source: "qrc:/image/assets/device_thermostat_24dp_1F1F1F_FILL0_wght400_GRAD0_opsz24.png"
            width: 32
            height: width
            anchors.centerIn: tempBar
            fillMode: Image.PreserveAspectFit
        }
    }
    ////////CÁC ICON BÁO STATE HIỆN TẠI Ở MÀN HÌNH/////////
    Rectangle
        {
            id: rectButton
            anchors.top: rectDisplay.bottom
            anchors.bottom: parent.bottom
            anchors.left: rectFuel.right
            anchors.right: rectTemp.left
            anchors.margins: 20
            color: "transparent"
            RowLayout
            {
                anchors.fill: parent
                spacing: 20
                Mybutton
                {
                    id:btnChart
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    buttonColor: mode === "chart" ? "red" : "lightgray"
                    btnradius: 20
                    iconSource: "qrc:/image/assets/chart.png"
                }
                Mybutton
                {
                    id:btnNormal
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    buttonColor: mode === "normal" ? "red" : "lightgray"
                    btnradius: 20
                    iconSource: "qrc:/image/assets/nornal.png"

                }
                Mybutton
                {
                    id:btnCan
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    buttonColor: mode === "can" ? "red" : "lightgray"
                    btnradius: 20
                    iconSource: "qrc:/image/assets/error.png"

                }
            }
        }
    ////////////////////////////////////////////////////////////////////
    /////////////VÙNG ĐIỀU KHIỂN GIẢ LẬP////////////////////
    Rectangle
    {
        id: keyArea
        anchors.fill: parent
        color: "transparent"
        focus: true
        Component.onCompleted:
        {
            keyArea.forceActiveFocus()
        }

        Keys.onPressed: (event)=> {
                            //if (event.isAutoRepeat) return
                            if (event.key === Qt.Key_Up) ecuCmd.throttleOn()
                            if (event.key === Qt.Key_Down) ecuCmd.throttleOff()
                            if (event.key === Qt.Key_Left)  ecuCmd.brakeOn()
                            if (event.key === Qt.Key_Right) ecuCmd.brakeOff()
                            if (event.key === Qt.Key_1) num1 = !num1
                            if (event.key === Qt.Key_2) num2 = !num2
                            if (event.key === Qt.Key_3) num3 = !num3
                            if (event.key === Qt.Key_4) num4 = !num4
                            if (event.key === Qt.Key_5) num5 = !num5
                            if (event.key === Qt.Key_6) num6 = !num6
                            if (event.key === Qt.Key_7) num7 = !num7
                            // if (event.key === Qt.Key_W) {
                            //     progressFuel += 0.1
                            //     if (progressFuel > 1) progressFuel = 1
                            // }
                            // if (event.key === Qt.Key_S) {
                            //     progressFuel -= 0.1
                            //     if (progressFuel <0) progressFuel = 0
                            // }
                            // if (event.key === Qt.Key_D) {
                            //     progressTemp += 0.1
                            //     if (progressTemp > 1) progressTemp = 1
                            // }
                            // if (event.key === Qt.Key_A) {
                            //     progressTemp -= 0.1
                            //     if (progressTemp < 0) progressTemp = 0
                            // }
                            if (event.key === Qt.Key_D) {
                                root.stateIndex++
                                if (stateIndex > 2)
                                root.stateIndex = 0
                                root.updateState()
                            }
                            if (event.key === Qt.Key_A) {
                                root.stateIndex--
                                if (stateIndex < 0)
                                root.stateIndex = 2
                                root.updateState()
                            }
                        }
    }
}

