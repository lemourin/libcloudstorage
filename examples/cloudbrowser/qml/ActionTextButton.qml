import QtQuick 2.0
import QtQuick.Controls 1.4

MouseArea {
    property string text
    property string textfield
    property string displaytext: textField.displayText
    property string placeholdertext
    property int minsize: 100

    id: button
    width: container.width
    height: container.height
    anchors.margins: padding
    Rectangle {
        id: container
        anchors.margins: padding
        border.color: "black"
        width: Math.max(minsize,
                        Math.max(text.width, textField.width) + padding)
        height: Math.max(minsize, text.height + textField.height + padding)
        color: "grey"
        radius: padding
        Item {
            anchors.centerIn: parent
            width: childrenRect.width
            height: childrenRect.height
            Text {
                id: text
                text: button.text
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: textField.horizontalCenter
            }
            TextField {
                id: textField
                text: textfield
                placeholderText: placeholdertext
                anchors.top: text.bottom
                width: Math.max(text.width, Math.max(0.2 * root.width, minsize))
            }
        }
    }
}
