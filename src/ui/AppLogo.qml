import QtQuick

// The app's mark, with its outline in the theme's text color so it reads on
// every Omarchy theme (see LogoProvider).
Image {
    source: "image://logo/" + theme.style.text
    sourceSize: Qt.size(width * 2, height * 2)
    smooth: true
    mipmap: true
    fillMode: Image.PreserveAspectFit
}
