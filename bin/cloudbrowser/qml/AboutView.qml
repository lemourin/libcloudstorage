import QtQuick 2.4
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.1 as Kirigami

Kirigami.ScrollablePage {
  anchors.fill: parent
  title: "Credits"

  ListView {
    model: ListModel {
      ListElement {
        name: "libcloudstorage"
        url: "https://code.videolan.org/videolan/libcloudstorage"
        icon_source: "qrc:/resources/vlc.png"
      }
      ListElement {
        name: "Kirigami 2"
        url: "https://github.com/KDE/kirigami"
        icon_source: "qrc:/resources/kde.png"
      }
      ListElement {
        name: "Qt"
        url: "https://www.qt.io"
        icon_source: "qrc:/resources/qt.png"
      }
      ListElement {
        name: "ffmpegthumbnailer"
        url: "https://github.com/dirkvdb/ffmpegthumbnailer"
        icon_source: "qrc:/resources/ffmpeg.png"
      }
      ListElement {
        name: "vlc-qt"
        url: "https://github.com/vlc-qt/vlc-qt"
        icon_source: "qrc:/resources/vlc-qt.png"
      }
      ListElement {
        name: "mega sdk"
        url: "https://github.com/meganz/sdk"
        icon_source: "qrc:/resources/providers/mega.png"
      }
      ListElement {
        name: "libmicrohttpd"
        url: "https://www.gnu.org/software/libmicrohttpd"
        icon_source: "qrc:/resources/gnu.png"
      }
      ListElement {
        name: "curl"
        url: "https://curl.haxx.se"
        icon_source: "qrc:/resources/curl.png"
      }
      ListElement {
        name: "jsoncpp"
        url: "https://github.com/open-source-parsers/jsoncpp"
        icon_source: "qrc:/resources/jsoncpp.png"
      }
      ListElement {
        name: "tinyxml2"
        url: "https://github.com/open-source-parsers/jsoncpp"
        icon_source: "qrc:/resources/tinyxml2.png"
      }
      ListElement {
        name: "crypto++"
        url: "https://www.cryptopp.com"
        icon_source: "qrc:/resources/cryptopp.png"
      }
    }
    delegate: Kirigami.BasicListItem {
      onClicked: Qt.openUrlExternally(url)
      icon: ""
      RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        Item {
          width: 150
          height: 50
          Image {
            anchors.centerIn: parent
            anchors.fill: parent
            mipmap: true
            source: icon_source
            fillMode: Image.PreserveAspectFit
          }
        }
        Label {
          anchors.right: parent.right
          anchors.margins: 15
          text: name
          color: Kirigami.Theme.textColor
        }
      }
    }
  }
}
