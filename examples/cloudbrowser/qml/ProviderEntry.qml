import QtQuick 2.7
import org.kde.kirigami 2.1 as Kirigami

Row {
  property string provider
  property string label
  property alias image_width: item.width
  property alias image_height: item.height

  Item {
    id: item
    Image {
      anchors.centerIn: parent
      fillMode: Image.PreserveAspectFit
      mipmap: true
      smooth: true
      height: parent.height
      source: "qrc:/resources/providers/" + provider + ".png"
    }
  }
  Kirigami.Label {
    anchors.verticalCenter: parent.verticalCenter
    text: label === "" ? cloud.pretty(provider) : label
  }
}
