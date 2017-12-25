package org.videolan.cloudbrowser;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class AuthView extends Activity {
    public AuthView() {
        CloudBrowser.m_auth_view = this;
    }

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);

        WebView webview = new WebView(this);
        webview.getSettings().setJavaScriptEnabled(true);
        webview.getSettings().setUserAgentString(
            "Mozilla/5.0 (Linux; Android 4.0.4; Galaxy Nexus Build/IMM76B) " +
            "AppleWebKit/535.19 (KHTML, like Gecko) " +
            "Chrome/18.0.1025.133 Mobile Safari/535.19");
        webview.getSettings().setBuiltInZoomControls(true);
        webview.getSettings().setDisplayZoomControls(false);
        webview.setWebViewClient(new WebViewClient());
        webview.loadUrl(getIntent().getExtras().getCharSequence("address").toString());
        setContentView(webview);
    }
}
