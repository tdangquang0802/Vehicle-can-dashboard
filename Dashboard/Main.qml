import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

ApplicationWindow {
    id: win
    width: 1280; height: 720; visible: true
    title: "Vehicle CAN Dashboard"
    color: "#0a0e17"

    readonly property color panel: "#111826"
    readonly property color line: "#1c2433"
    readonly property color dim: "#64748b"
    readonly property color cyan: "#22d3ee"
    readonly property color green: "#34d399"
    readonly property color amber: "#fbbf24"
    readonly property color red: "#f87171"
    readonly property string mono: "Consolas, 'JetBrains Mono', monospace"
    readonly property var phases: ["IDLE", "ACCEL", "CRUISE", "DECEL"]
    readonly property var phaseColors: [dim, amber, green, cyan]

    component Card: Rectangle { color: win.panel; radius: 14; border.color: win.line }
    component Stat: ColumnLayout {
        property string k; property string v; property color c: "#f1f5f9"
        spacing: 2
        Text { text: k; color: win.dim; font.pixelSize: 11; font.letterSpacing: 1.5 }
        Text { text: v; color: c; font.pixelSize: 22; font.family: win.mono; font.weight: Font.DemiBold }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 16

        // ---------- Top bar ----------
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text { text: "VEHICLE CAN"; color: "#f1f5f9"; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 3 }
            Text { text: "GATEWAY DASHBOARD"; color: win.dim; font.pixelSize: 20; font.letterSpacing: 3 }
            Item { Layout.fillWidth: true }
            Repeater {
                model: [
                    { t: carbackend.connected ? carbackend.portName + " · 115200" : "PORT CLOSED", c: carbackend.connected ? win.green : win.red },
                    { t: carbackend.linkOk ? "CAN LINK OK" : "CAN STALE", c: carbackend.linkOk ? win.green : win.red },
                    { t: carbackend.faultCode ? "FAULT 0x" + carbackend.faultCode.toString(16).toUpperCase().padStart(2, "0") : "NO FAULT",
                      c: carbackend.faultCode ? win.red : win.green }
                ]
                delegate: Rectangle {
                    height: 30; width: chip.implicitWidth + 36; radius: 15
                    color: Qt.rgba(modelData.c.r, modelData.c.g, modelData.c.b, 0.12)
                    border.color: modelData.c
                    Row { anchors.centerIn: parent; spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4; color: modelData.c; anchors.verticalCenter: parent.verticalCenter }
                        Text { id: chip; text: modelData.t; color: modelData.c; font.pixelSize: 12; font.family: win.mono } }
                }
            }
        }

        // ---------- Gauges + stats ----------
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 300; spacing: 16

            Card { Layout.preferredWidth: 340; Layout.fillHeight: true
                ArcGauge { anchors.fill: parent; anchors.margins: 14
                    value: carbackend.speed; maxValue: 120; unit: "KM/H"; label: "SPEED"; accent: win.cyan } }
            Card { Layout.preferredWidth: 340; Layout.fillHeight: true
                ArcGauge { anchors.fill: parent; anchors.margins: 14
                    value: carbackend.rpm; maxValue: 1200; unit: "RPM"; label: "WHEEL"; accent: "#a78bfa" } }

            Card {
                Layout.fillWidth: true; Layout.fillHeight: true
                GridLayout {
                    anchors.fill: parent; anchors.margins: 22; columns: 2; rowSpacing: 18; columnSpacing: 24
                    Stat { k: "PHASE"; v: win.phases[carbackend.phase] || "—"; c: win.phaseColors[carbackend.phase] || win.dim }
                    Stat { k: "CYCLE TIME"; v: carbackend.cycleTime + " s" }
                    Stat { k: "SEQ"; v: carbackend.seq }
                    Stat { k: "FRAME RATE"; v: carbackend.fps + " /s" }
                    Stat { k: "TOTAL FRAMES"; v: carbackend.totalFrames }
                    Stat { k: "CRC ERRORS"; v: carbackend.crcErrors; c: carbackend.crcErrors ? win.red : win.green }
                    // cycle progress (95 s loop)
                    ColumnLayout { Layout.columnSpan: 2; Layout.fillWidth: true; spacing: 6
                        Text { text: "CYCLE PROGRESS"; color: win.dim; font.pixelSize: 11; font.letterSpacing: 1.5 }
                        Rectangle { Layout.fillWidth: true; height: 6; radius: 3; color: win.line
                            Rectangle { height: parent.height; radius: 3; color: win.cyan
                                width: parent.width * Math.min(1, carbackend.cycleTime / 95)
                                Behavior on width { NumberAnimation { duration: 300 } } } } }
                }
            }
        }

        // ---------- CAN frame monitor ----------
        Card {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 8
                RowLayout {
                    Text { text: "CAN FRAME MONITOR"; color: "#f1f5f9"; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 2 }
                    Text { text: "reconstructed from gateway UART"; color: win.dim; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "CLEAR"; onClicked: carbackend.clearLog()
                        contentItem: Text { text: parent.text; color: win.dim; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                        background: Rectangle { implicitWidth: 64; implicitHeight: 24; radius: 6; color: "transparent"; border.color: win.line }
                    }
                }
                RowLayout {   // header
                    Layout.fillWidth: true; spacing: 0
                    Repeater { model: [["TIME", 120], ["ID", 80], ["TYPE", 160], ["DLC", 50], ["DATA (HEX)", 330]]
                        delegate: Text { Layout.preferredWidth: modelData[1]; text: modelData[0]; color: win.dim; font.pixelSize: 11; font.letterSpacing: 1.5 } }
                    Text { text: "STATUS"; color: win.dim; font.pixelSize: 11; font.letterSpacing: 1.5 }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: win.line }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: carbackend.frames
                    delegate: Rectangle {
                        width: ListView.view.width; height: 26; radius: 4
                        color: modelData.ok ? (index % 2 ? "transparent" : "#0e1522") : "#2a1216"
                        RowLayout {
                            anchors.fill: parent; spacing: 0
                            component Cell: Text { color: "#cbd5e1"; font.pixelSize: 13; font.family: win.mono; verticalAlignment: Text.AlignVCenter }
                            Cell { Layout.preferredWidth: 120; text: modelData.time; color: win.dim }
                            Cell { Layout.preferredWidth: 80; text: modelData.id; color: win.cyan }
                            Cell { Layout.preferredWidth: 160; text: modelData.name }
                            Cell { Layout.preferredWidth: 50; text: modelData.dlc }
                            Cell { Layout.preferredWidth: 330; text: modelData.data }
                            Cell { text: modelData.ok ? "OK" : "ERR"; color: modelData.ok ? win.green : win.red }
                        }
                    }
                    Text { anchors.centerIn: parent; visible: parent.count === 0; text: "Waiting for frames…"; color: win.dim }
                }
            }
        }
    }
}
