import QtQuick 2.0

MouseArea {
    property string text

    id: root
    width: container.width
    height: container.height
    anchors.margins: padding
    Rectangle {
        id: container
        anchors.margins: padding
        border.color: "black"
        width: Math.max(100, text.width + padding)
        height: Math.max(100, text.height + padding)
        color: "grey"
        radius: padding
        Text {
            id: text
            anchors.centerIn: parent
            text: root.text
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
