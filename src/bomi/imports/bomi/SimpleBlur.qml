import QtQuick 2.0

Item {
    property Item source: Item { }
    property size textureSize: source.textureSize
        ? source.textureSize : Qt.size(source.width, source.height)

    ShaderEffect {
        id: pass1
        readonly property Item src: source
        property real dx: 1.0 / textureSize.width
        anchors.fill: parent; visible: false
        fragmentShader: "qrc:/shaders/blur_h.frag.qsb"
    }

    ShaderEffectSource {
        id: interm
        sourceItem: pass1; visible: false
        anchors.fill: parent
        textureSize: parent.textureSize
        wrapMode: ShaderEffectSource.ClampToEdge
    }

    ShaderEffect {
        id: pass2
        readonly property Item src: interm
        property real dy: 1.0 / textureSize.height
        anchors.fill: parent
        fragmentShader: "qrc:/shaders/blur_v.frag.qsb"
    }
}
