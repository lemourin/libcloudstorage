package org.videolan.cloudbrowser;

import android.os.Bundle;
import android.content.Intent;
import android.content.res.Configuration;
import android.view.WindowManager;
import android.database.Cursor;
import android.provider.OpenableColumns;
import android.net.Uri;
import android.content.pm.ActivityInfo;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import org.qtproject.qt5.android.bindings.QtActivity;

import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdListener;

public class CloudBrowser extends QtActivity {
    private NotificationHelper m_notification_helper;
    private ViewGroup m_view_group;
    private RelativeLayout m_ad_view;
    private boolean m_ad_view_visible = false;

    private void updateAdView() {
        if (m_ad_view != null && m_view_group != null)
            m_view_group.removeView(m_ad_view);
        if (!m_ad_view_visible) return;
        final AdView ad = new AdView(this);
        ad.setAdUnitId("ca-app-pub-9966958697007244/9093813086");
        ad.setAdSize(new AdSize(AdSize.FULL_WIDTH, 50));
        ad.setAdListener(new AdListener() {
            private boolean m_loaded;

            void attach() {
                if (!m_loaded && m_ad_view_visible) {
                    m_loaded = true;
                    m_ad_view = new RelativeLayout(CloudBrowser.this);
                    RelativeLayout.LayoutParams ad_layout_params =
                        new RelativeLayout.LayoutParams(
                            RelativeLayout.LayoutParams.FILL_PARENT,
                            RelativeLayout.LayoutParams.WRAP_CONTENT);
                    ad_layout_params.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
                    m_ad_view.addView(ad, ad_layout_params);
                    m_view_group.addView(m_ad_view, new RelativeLayout.LayoutParams(
                        RelativeLayout.LayoutParams.FILL_PARENT,
                        RelativeLayout.LayoutParams.FILL_PARENT));
                }
            }

            public void onAdLoaded() {
                runOnUiThread(new Runnable() {
                    public void run() {
                        Utility.onActionRequested("SHOW_AD");
                        attach();
                    }
                });
            }

            public void onAdFailedToLoad(int error) {
                Utility.onActionRequested("HIDE_AD");
            }
        });
        ad.loadAd(new AdRequest.Builder().build());
    }

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);

        m_notification_helper = new NotificationHelper(this);
        MobileAds.initialize(this, "ca-app-pub-9966958697007244~3913638593");

        View view = getWindow().getDecorView().getRootView();
        if (view instanceof ViewGroup) {
            m_view_group = (ViewGroup) view;
            updateAdView();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        updateAdView();
    }

    @Override
    public void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        Utility.onRequestResult(request, result, data);
    }

    @Override
    public void onDestroy() {
        m_notification_helper.release();
        System.exit(0);
        super.onDestroy();
    }

    public void showAd() {
        runOnUiThread(new Runnable() {
            public void run() {
                m_ad_view_visible = true;
                updateAdView();
            }
        });
    }

    public void hideAd() {
        runOnUiThread(new Runnable() {
            public void run() {
                m_ad_view_visible = false;
                if (m_ad_view != null)
                    m_ad_view.setVisibility(View.INVISIBLE);
            }
        });
    }

    public void setDefaultOrientation() {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    public void setLandScapeOrientation() {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    public void enableKeepScreenOn() {
        runOnUiThread(new Runnable() {
            public void run() {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public void disableKeepScreenOn() {
        runOnUiThread(new Runnable() {
            public void run() {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public Intent openWebPage(String url) {
        Intent intent = new Intent(this, AuthView.class);
        intent.putExtra("address", url);
        return intent;
    }

    public void closeWebPage(Intent i) {
        if (AuthView.m_current != null) {
            Intent intent = new Intent(this, AuthView.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            AuthView.m_current.startActivity(intent);
            AuthView.m_current.finish();
            AuthView.m_current = null;
        }
    }

    public NotificationHelper notification() {
        return m_notification_helper;
    }

    public String filename(Uri uri) {
        String result = "";
        try (Cursor cursor = getContentResolver().
                query(uri, null, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst())
                result = cursor.getString(
                    cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
        }
        return result;
    }
}
