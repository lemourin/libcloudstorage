import QtQuick 2.7
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.1 as Kirigami

Kirigami.ScrollablePage {
  id: page
  anchors.fill: parent
  title: "Add Provider"

  Component {
    id: webview
    Kirigami.Page {
      property string url: ""
      id: webview_page
      title: "Authorization"
      anchors.fill: parent
      WebView {
        id: view
        anchors.fill: parent
        url: webview_page.url
        /*onContentsSizeChanged: {
          if (parent.width < view.contentsSize.width)
            zoomFactor = parent.width / view.contentsSize.width;
        } */
      }
    }
  }

  Connections {
    target: cloud
    onReceivedCode: {
      root.pageStack.pop();
    }
  }

  ListView {
    model: cloud.providers
    delegate: Kirigami.BasicListItem {
      onClicked: {
        root.pageStack.push(webview, {url: cloud.authorizationUrl(modelData)});
      }
      contentItem: ProviderEntry {
        image_width: 120
        image_height: 35
        provider: modelData
      }
    }
  }
}
