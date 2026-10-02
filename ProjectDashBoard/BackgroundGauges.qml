import QtQuick
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

Item {
    id: root
    width: 300
    height: 300
    property bool isLeft: true

    Image {
        id: bgTexture
        source: "assets/images/background_meter.jpg"
        anchors.fill: parent
        fillMode: Image.Stretch
        visible: false
    }

    transform: Scale {
        origin.x: root.width / 2
        origin.y: root.height / 2
        xScale: root.isLeft ? 1 : -1
    }

    Shape {
        id: customShapeMask
        anchors.fill: parent
        visible: false
        layer.enabled: true

        ShapePath {
            fillColor: "black"
            strokeColor: "transparent"
            strokeWidth: 0
            capStyle: ShapePath.RoundCap

            startX: customShapeMask.width / 2
            startY: customShapeMask.height / 2

            PathAngleArc {
                centerX: customShapeMask.width / 2
                centerY: customShapeMask.height / 2
                radiusX: (customShapeMask.width / 2) - 4
                radiusY: (customShapeMask.height / 2) - 4

                startAngle: 55
                sweepAngle: -190

                moveToStart: true
            }

            PathLine {
                x: customShapeMask.width / 2
                y: customShapeMask.height / 2
            }
        }
    }

    OpacityMask {
        anchors.fill: parent
        source: bgTexture
        maskSource: customShapeMask
    }
}
