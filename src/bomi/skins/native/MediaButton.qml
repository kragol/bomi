import QtQuick 2.0
import QtQuick.Controls
import bomi 1.0 as B

ToolButton {
    id: button
    // The theme icon name; Controls 2 buttons already have an icon group property.
    property string iconName
    property string action
    property string fallback: "oxygen"
    property QtObject __action: B.App.action(action)
    icon.name: iconName
    icon.source: fallback + "/" + iconName + ".png"
    display: AbstractButton.IconOnly
    ToolTip.visible: hovered && ToolTip.text.length > 0
    ToolTip.text: __action ? __action.text : ""
    onClicked: B.App.execute(action)
    anchors.verticalCenter: parent.verticalCenter
    enabled: __action ? __action.enabled : false
}
