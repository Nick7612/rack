import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Material's Button hardcodes full-pill corners, which would ignore the
// desktop's window rounding. This keeps every button on the Omarchy radius.
Button {
    Material.roundedScale: theme.radius >= 16 ? Material.LargeScale
                          : theme.radius >= 12 ? Material.MediumScale
                          : theme.radius >= 8 ? Material.SmallScale
                          : theme.radius >= 4 ? Material.ExtraSmallScale
                          : Material.NotRounded
}
