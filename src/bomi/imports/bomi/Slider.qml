import QtQuick 2.0
import bomi 1.0

// A horizontal slider drawn by a SliderStyle, with the layout of the
// QtQuick.Controls 1.x Slider it replaces (Qt 6 has no Controls 1): the groove
// spans the width, centred vertically, and the handle travels from the left
// edge to the right edge as rate goes from 0 to 1.
Item {
    id: seeker
    property real ahead: 0
    property real min: 0
    property real max: 1
    property real value: 0
    property Component style
    property int orientation: Qt.Horizontal // only horizontal is drawn
    readonly property alias range: slider.range
    readonly property alias rate: slider.rate
    readonly property alias arate: slider.arate
    readonly property alias hpressed: slider.hpressed
    readonly property alias hhovered: slider.hhovered
    property bool acceptsWheel: false

    property alias __hpressed: slider.hpressed
    property alias __hhovered: slider.hhovered

    implicitHeight: Math.max(groove.height, handle.height)
    implicitWidth: 100

    Item {
        id: slider
        anchors.fill: parent
        readonly property real minimumValue: seeker.min
        readonly property real maximumValue: seeker.max
        readonly property real value: seeker.value
        readonly property real min: seeker.min
        readonly property real max: seeker.max
        readonly property real ahead: Math.min(seeker.ahead, max)
        readonly property real range: max - min
        readonly property real rate: range > 0 ? (value - min)/range : 0
        readonly property real arate: ahead == 0 || range <= 0 ? 0 : (ahead - min)/range
        readonly property bool pressed: mouseArea.pressed
        readonly property bool hovered: mouseArea.containsMouse
        property bool hpressed: pressed
        property bool hhovered: hovered
        property QtObject __style: null

        Loader {
            id: groove
            width: slider.width // the groove item gets this width, keeps its own height
            y: Math.round((slider.height - height) * 0.5)
            sourceComponent: slider.__style && slider.__style.groove ? slider.__style.groove : defaultGroove
        }
        Loader {
            id: handle
            anchors.verticalCenter: groove.verticalCenter
            x: Math.round(slider.rate * (slider.width - width))
            sourceComponent: slider.__style && slider.__style.handle ? slider.__style.handle : defaultHandle
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            function valueAt(x) {
                var w = slider.width - handle.width
                var r = w > 0 ? Math.max(0, Math.min(1, (x - handle.width * 0.5) / w)) : 0
                return seeker.min + r * slider.range
            }
            onPressed: (mouse) => { seeker.value = valueAt(mouse.x) }
            onPositionChanged: (mouse) => { if (pressed) seeker.value = valueAt(mouse.x) }
            onWheel: (wheel) => {
                // Like Controls 1: a wheel notch moves the handle by 15 pixels.
                if (acceptsWheel && slider.width > 0) {
                    var v = seeker.value + wheel.angleDelta.y / 8 * slider.range / slider.width
                    seeker.value = Math.max(seeker.min, Math.min(seeker.max, v))
                }
                wheel.accepted = true
            }
        }
    }

    function __updateStyle() {
        if (slider.__style)
            slider.__style.destroy()
        slider.__style = style ? style.createObject(slider, { control: slider }) : null
    }
    onStyleChanged: __updateStyle()
    Component.onCompleted: __updateStyle()

    Component {
        id: defaultGroove
        Rectangle {
            width: slider.width; height: 4; radius: 2
            color: Qt.rgba(0.5, 0.5, 0.5, 0.6)
            Rectangle {
                width: parent.width * slider.arate; height: parent.height; radius: parent.radius
                color: Qt.rgba(1, 1, 1, 0.3)
            }
            Rectangle {
                width: parent.width * slider.rate; height: parent.height; radius: parent.radius
                color: "#3daee9"
            }
        }
    }
    Component {
        id: defaultHandle
        Rectangle {
            width: 12; height: 12; radius: 6
            color: slider.pressed ? "#3daee9" : "white"
            border { width: 1; color: Qt.rgba(0, 0, 0, 0.4) }
        }
    }
}
