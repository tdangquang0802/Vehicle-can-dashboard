import QtQuick

Item {
    id: g
    property real value: 0
    property real maxValue: 100
    property string label: ""
    property string unit: ""
    property color accent: "#22d3ee"
    property real shown: value
    Behavior on shown { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    onShownChanged: arc.requestPaint()
    onAccentChanged: arc.requestPaint()

    Canvas {
        id: arc
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            var c = getContext("2d"); c.reset()
            var r = Math.min(width, height) / 2 - 14, cx = width / 2, cy = height / 2
            var a0 = Math.PI * 0.75, span = Math.PI * 1.5
            var f = Math.max(0, Math.min(1, g.shown / g.maxValue))
            c.lineWidth = 12; c.lineCap = "round"
            c.strokeStyle = "#1c2433"; c.beginPath(); c.arc(cx, cy, r, a0, a0 + span); c.stroke()
            c.strokeStyle = g.accent;  c.beginPath(); c.arc(cx, cy, r, a0, a0 + span * f); c.stroke()
        }
    }
    Column {
        anchors.centerIn: parent
        spacing: 0
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: Math.round(g.shown)
               color: "#f1f5f9"; font.pixelSize: g.height * 0.26; font.weight: Font.DemiBold }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: g.unit
               color: g.accent; font.pixelSize: 15; font.letterSpacing: 2 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: g.label
               color: "#64748b"; font.pixelSize: 12; font.letterSpacing: 2 }
    }
}
