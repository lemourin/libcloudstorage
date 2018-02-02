import QtQuick 2.7
import QtQuick.Templates 2.0 as Templates
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
  Templates.Label {
    anchors.verticalCenter: parent.verticalCenter
    color: Kirigami.Theme.textColor
    text: "<b>" + cloud.pretty(provider) + "</b>" + (label !== "" ?
            "<br/><div>" + label + "</div>" : "")
  }
}
