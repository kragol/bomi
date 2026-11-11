import QtQuick
import HdrWindow

Window {
    id: win
    width: 1280
    height: 720
    visible: true
    color: "black"
    title: "Qt 6 HDR test — " + mode + ", UI " + uiMode

    required property string mode
    // How the QML UI reaches the PQ window: "off" (drawn as is, so white is
    // 10000 nits), "layer" (UI layer converted to PQ, Qt blends on PQ values)
    // or "composite" (one pass blends video and UI as SDR does, then PQ).
    required property string uiMode
    readonly property bool composite: uiMode === "composite"

    MpvItem {
        id: mpv
        anchors.fill: parent
        focus: true

        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Space: mpv.command(["cycle", "pause"]); break
            case Qt.Key_Left: mpv.command(["seek", "-5"]); break
            case Qt.Key_Right: mpv.command(["seek", "5"]); break
            case Qt.Key_Up: mpv.command(["seek", "30"]); break
            case Qt.Key_Down: mpv.command(["seek", "-30"]); break
            case Qt.Key_F:
                win.visibility = win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
                break
            case Qt.Key_I: ui.visible = !ui.visible; break
            case Qt.Key_Q:
            case Qt.Key_Escape: Qt.quit(); break
            default: return
            }
            event.accepted = true
        }
    }

    // Everything Qt Quick draws over the video, standing in for bomi's OSD,
    // controls and playlist. Coordinates are fixed so frame grabs can be
    // measured (see measure_ui.py).
    Item {
        id: ui
        anchors.fill: parent
        layer.enabled: win.uiMode === "layer"
        layer.effect: ShaderEffect { fragmentShader: "qrc:/shaders/ui_layer.frag.qsb" }

        // Opaque grey ramp, sRGB 0, 0.1 ... 1.0
        Row {
            x: 20; y: 20; spacing: 10
            Repeater {
                model: 11
                Rectangle { width: 40; height: 40; color: Qt.rgba(index / 10, index / 10, index / 10, 1) }
            }
        }
        // Opaque colours, as in skin icons
        Row {
            x: 20; y: 80; spacing: 10
            Repeater {
                model: ["#ff0000", "#00ff00", "#0000ff", "#e0473c", "#3daee9"]
                Rectangle { width: 40; height: 40; radius: 20; color: modelData }
            }
        }
        // Translucent black over every band of the video, like the OSD box
        // (#c0000000) and the playlist background (55%)
        Rectangle { x: 0; y: 300; width: parent.width; height: 50; color: "#c0000000" }
        Rectangle { x: 0; y: 360; width: parent.width; height: 50; color: Qt.rgba(0, 0, 0, 0.55) }
        // Translucent white, like the playlist's current-row highlight
        Rectangle { x: 0; y: 420; width: parent.width; height: 50; color: Qt.rgba(1, 1, 1, 0.3) }

        // A playlist-style drawer, for looking at
        Rectangle {
            anchors { right: parent.right; bottom: parent.bottom }
            width: 360; height: 220
            color: Qt.rgba(0, 0, 0, 0.55)
            Column {
                x: 16; y: 12; spacing: 6
                Repeater {
                    model: ["1. Soulm8te (2026)", "2. Some other film", "3. A third one"]
                    Rectangle {
                        width: 328; height: 36; radius: 3
                        color: index === 0 ? Qt.rgba(1, 1, 1, 0.3) : "transparent"
                        Text { anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                               text: modelData; color: "white"; font.pixelSize: 17 }
                    }
                }
            }
        }

        Rectangle {
            id: info
            anchors { left: parent.left; bottom: parent.bottom; margins: 20 }
            width: label.implicitWidth + 24
            height: label.implicitHeight + 16
            color: "#c0000000"
            radius: 6
            Text {
                id: label
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 18
                text: win.mode + ", UI " + win.uiMode + " — I hides the UI, F fullscreen, Space pause, Q quits"
            }
        }
    }

    // --ui composite: the video and the UI are only drawn through this pass.
    ShaderEffectSource {
        id: videoSource
        sourceItem: win.composite ? mpv : null
        hideSource: true
        format: ShaderEffectSource.RGBA16F
        visible: false
    }
    ShaderEffectSource {
        id: uiSource
        sourceItem: win.composite ? ui : null
        hideSource: true
        visible: false
    }
    ShaderEffect {
        anchors.fill: parent
        visible: win.composite
        property var video: videoSource
        property var ui: uiSource
        fragmentShader: "qrc:/shaders/ui_composite.frag.qsb"
    }
}
