import QtQuick

// The app's mark: an easel on a VT100-style terminal. It carries its own contrast,
// so it shows in full color on every theme.
Image {
    source: "qrc:/assets/logo.png"
    sourceSize.height: height * 2
    smooth: true
    mipmap: true
    fillMode: Image.PreserveAspectFit
}
