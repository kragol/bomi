import QtQuick
import HdrWindow

Window {
    id: win
    width: 1280
    height: 720
    visible: true
    color: "black"
    title: "Qt 6 HDR test — " + mode

    required property string mode

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
            case Qt.Key_I: info.visible = !info.visible; break
            case Qt.Key_Q:
            case Qt.Key_Escape: Qt.quit(); break
            default: return
            }
            event.accepted = true
        }
    }

    // A plain white QML overlay, drawn by Qt Quick, not mpv. In a PQ-tagged
    // window, white (1.0) means 10000 nits, so this shows what bomi's OSD and
    // playlist would look like without converting the UI to PQ.
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
            text: win.mode + " — QML overlay in plain white (I hides, F fullscreen, Space pause, Q quits)"
        }
    }
}
