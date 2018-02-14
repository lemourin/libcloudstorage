package org.videolan.cloudbrowser;

import android.net.Uri;
import android.content.Intent;

public class Utility {
    public static Uri stringToUri(String uri) {
        String basename = uri.substring(0, uri.lastIndexOf('/') + 1);
        String filename = uri.substring(uri.lastIndexOf('/') + 1);
        return Uri.parse(basename + Uri.encode(Uri.decode(filename)));
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

}
