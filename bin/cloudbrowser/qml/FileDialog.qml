import QtQuick 2.0

Loader {
  property bool selectExisting
  property string filename
  property string fileUrl

  signal accepted()

  function dialog(name) {
    if (name === "android")
      return "AndroidFileDialog.qml";
    else if (name === "winrt")
      return "WinRTFileDialog.qml";
    else
      return "QtFileDialog.qml";
  }

  id: loader
  source: dialog(platform.name())
  asynchronous: false

  onLoaded: {
    item.selectExisting = Qt.binding(function() { return selectExisting; });
    item.filename = Qt.binding(function() { return filename; });
  }

  Connections {
    target: loader.item
    onReady: {
      loader.fileUrl = target.url;
      loader.filename = target.filename;
      accepted();
    }
  }

  function open() {
    item.open();
  }
}
