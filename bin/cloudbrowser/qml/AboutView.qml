import QtQuick 2.4
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.1 as Kirigami

Kirigami.ScrollablePage {
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
        name: "FFmpeg"
        url: "https://www.ffmpeg.org/"
        icon_source: "qrc:/resources/ffmpeg.png"
      }
      ListElement {
        name: "MPV"
        url: "https://mpv.io/"
        icon_source: "qrc:/resources/mpv.png"
      }
      ListElement {
        name: "VLC"
        url: "https://www.videolan.org/vlc/index.html"
        icon_source: "qrc:/resources/vlc.png"
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
        url: "https://github.com/leethomason/tinyxml2"
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
      reserveSpaceForIcon: false
      RowLayout {
        Item {
          Layout.alignment: Qt.AlignLeft
          Layout.fillWidth: true
          height: 50
          Item {
            width: 150
            height: parent.height
            Image {
              anchors.fill: parent
              mipmap: true
              source: icon_source
              fillMode: Image.PreserveAspectFit
            }
          }
        }
        Label {
          Layout.alignment: Qt.AlignRight
          Layout.margins: 15
          text: name
          color: Kirigami.Theme.textColor
        }
      }
    }
  }
}
