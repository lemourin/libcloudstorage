package org.videolan.cloudbrowser;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class AuthView extends Activity {
    public AuthView() {
        CloudBrowser.m_authView = this;
    }

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);

        m_webview = new WebView(this);
        m_webview.getSettings().setJavaScriptEnabled(true);
        m_webview.getSettings().setUserAgentString(
            "Mozilla/5.0 (Linux; Android 4.0.4; Galaxy Nexus Build/IMM76B) " +
            "AppleWebKit/535.19 (KHTML, like Gecko) " +
            "Chrome/18.0.1025.133 Mobile Safari/535.19");
        m_webview.getSettings().setBuiltInZoomControls(true);
        m_webview.getSettings().setDisplayZoomControls(false);
        m_webview.setWebViewClient(new WebViewClient());
        m_webview.loadUrl(getIntent().getExtras().getCharSequence("address").toString());
        setContentView(m_webview);
    }

    private WebView m_webview;
}
