package org.videolan.cloudbrowser;

import android.os.Bundle;
import android.content.Intent;
import android.content.res.Configuration;
import android.view.WindowManager;
import android.database.Cursor;
import android.provider.OpenableColumns;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.content.pm.ActivityInfo;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import org.qtproject.qt5.android.bindings.QtActivity;

import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdListener;

public class CloudBrowser extends QtActivity {
    private static CloudBrowser m_instance;
    public static AuthView m_auth_view;
    private ViewGroup m_view_group;
    private RelativeLayout m_ad_view;
    private boolean m_ad_view_visible = false;

    public static class FileOutput {
        public ParcelFileDescriptor m_descriptor;
        public FileOutputStream m_stream;
    }

    public static class FileInput {
        public ParcelFileDescriptor m_descriptor;
        public FileInputStream m_stream;
    }

    public CloudBrowser() {
        m_instance = this;
    }

    private void updateAdView() {
        if (m_ad_view != null && m_view_group != null)
            m_view_group.removeView(m_ad_view);
        if (!m_ad_view_visible) return;
        final AdView ad = new AdView(this);
        ad.setAdUnitId("ca-app-pub-9966958697007244/9093813086");
        ad.setAdSize(new AdSize(AdSize.FULL_WIDTH, 50));
        ad.setAdListener(new AdListener() {
            private boolean m_loaded;

            public void attach() {
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
                        attach();
                    }
                });
            }
        });
        ad.loadAd(new AdRequest.Builder().build());
    }

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);

        NotificationHelper.initialize();
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
    public void onDestroy() {
        NotificationHelper.release();
        super.onDestroy();
    }

    public static CloudBrowser instance() {
        return m_instance;
    }

    public static void showAd() {
        m_instance.runOnUiThread(new Runnable() {
            public void run() {
                m_instance.m_ad_view_visible = true;
                m_instance.updateAdView();
            }
        });
    }

    public static void hideAd() {
        m_instance.runOnUiThread(new Runnable() {
            public void run() {
                m_instance.m_ad_view_visible = false;
                if (m_instance.m_ad_view != null)
                    m_instance.m_ad_view.setVisibility(View.INVISIBLE);
            }
        });
    }

    public static void setDefaultOrientation() {
        m_instance.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    public static void setLandScapeOrientation() {
        m_instance.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    public static void enableKeepScreenOn() {
        m_instance.runOnUiThread(new Runnable() {
            public void run() {
                m_instance.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public static void disableKeepScreenOn() {
        m_instance.runOnUiThread(new Runnable() {
            public void run() {
                m_instance.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); 
            }
        });
    }

    public static Intent openFileDialog() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        return intent;
    }

    public static Intent createFileDialog(String filename) {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.putExtra(Intent.EXTRA_TITLE, filename);
        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        return intent;
    }

    public static Intent openWebPage(String url) {
        Intent intent = new Intent(m_instance, AuthView.class);
        intent.putExtra("address", url);
        return intent;
    }

    public static void closeWebPage(Intent i) {
        if (m_auth_view != null) {
            Intent intent = new Intent(m_instance, AuthView.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            m_auth_view.startActivity(intent);
            m_auth_view.finish();
            m_auth_view = null;
        }
    }

    public static Uri stringToUri(String uri) {
        String basename = uri.substring(0, uri.lastIndexOf('/') + 1);
        String filename = uri.substring(uri.lastIndexOf('/') + 1);
        return Uri.parse(basename + Uri.encode(Uri.decode(filename)));
    }

    public static FileOutput openWrite(String uri) throws IOException {
        FileOutput file = new FileOutput();
        file.m_descriptor = m_instance.getContentResolver().
            openFileDescriptor(stringToUri(uri), "w");
        file.m_stream = new FileOutputStream(file.m_descriptor.getFileDescriptor());
        return file;
    }

    public static FileInput openRead(String uri) throws IOException {
        FileInput file = new FileInput();
        file.m_descriptor = m_instance.getContentResolver().
            openFileDescriptor(stringToUri(uri), "r");
        file.m_stream = new FileInputStream(file.m_descriptor.getFileDescriptor());
        return file;
    }

    public static void write(FileOutput file, byte[] data) throws IOException {
        file.m_stream.write(data);
    }

    public static byte[] read(FileInput file, int size) throws IOException {
        byte[] buffer = new byte[size];
        int nsize = file.m_stream.read(buffer);
        if (nsize == -1)
            return new byte[0];
        byte[] result = new byte[nsize];
        for (int i = 0; i < nsize; i++)
            result[i] = buffer[i];
        return result;
    }

    public static String filename(Uri uri) {
        Cursor cursor = m_instance.getContentResolver().
            query(uri, null, null, null, null, null);
        String result = "";
        try {
            if (cursor != null && cursor.moveToFirst())
                result = cursor.getString(
                    cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
        } finally {
            cursor.close();
        }
        return result;
    }

    public static long size(FileInput file) throws IOException {
        return file.m_descriptor.getStatSize();
    }

    public static boolean reset(FileInput file) throws IOException {
        file.m_stream.close();
        file.m_stream = new FileInputStream(file.m_descriptor.getFileDescriptor());
        return true;
    }

    public static void closeWrite(FileOutput file) throws IOException {
        file.m_stream.close();
        file.m_descriptor.close();
    }

    public static void closeRead(FileInput file) throws IOException {
        file.m_stream.close();
        file.m_descriptor.close();
    }
}
