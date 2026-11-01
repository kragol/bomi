import QtQuick 2.0

// The part of QtQuick.Controls.Styles 1.x SliderStyle that bomi's skins use.
// Slider instantiates its style once per slider and sets control to the item
// the components may read: width/height, rate, arate, value, minimumValue,
// maximumValue, pressed, hovered, hpressed, hhovered.
QtObject {
    property Item control
    property Component groove
    property Component handle
}
