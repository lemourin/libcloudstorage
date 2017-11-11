import QtQuick 2.0
import QtQuick.Dialogs 1.2

FileDialog {
  property string filename
  property string url
  
  signal ready()

  onAccepted: {
    if (selectFolder) {
      url = fileUrl + seperator + filename;
    } else {
      url = fileUrl;
      filename = url.substring(url.lastIndexOf('/') + 1);
    }
    ready();
  }
}
