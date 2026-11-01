import QtQuick 2.0
import QtQuick.Controls
import QtQuick.Layouts
import bomi 1.0 as B

ToolButton {
    id: button
    // The theme icon name; Controls 2 buttons already have an icon group property.
    property string iconName
    // The bomi action id; Controls 2 buttons already have a FINAL action property.
    property string actionId
    property string fallback: "oxygen"
    property QtObject __action: B.App.action(actionId)
    icon.name: iconName
    icon.source: fallback + "/" + iconName + ".png"
    display: AbstractButton.IconOnly
    ToolTip.visible: hovered && ToolTip.text.length > 0
    ToolTip.text: __action ? __action.text : ""
    onClicked: B.App.execute(actionId)
    Layout.alignment: Qt.AlignVCenter
    enabled: __action ? __action.enabled : false
}
