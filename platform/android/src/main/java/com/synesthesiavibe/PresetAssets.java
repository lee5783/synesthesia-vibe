//
//  PresetAssets.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.SystemClock;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Locale;

public final class PresetAssets {

    public static final String ROOT_DIRECTORY = "synesthesiavibe";
    public static final String PRESETS_DIRECTORY = "presets";
    public static final String TEXTURES_DIRECTORY = "textures";
    public static final String AUDIO_DIRECTORY = "audio";
    public static final String PRESET_EXTENSION = ".milk";

    private static final String LOG_TAG = "SynesthesiaVibe";
    private static final String VERSION_FILE = ".assets-version";
    private static final int COPY_BUFFER_BYTES = 64 * 1024;

    private PresetAssets() {
    }

    public static File install(Context context) throws IOException {
        return install(context, packageVersion(context));
    }

    public static synchronized File install(Context context, String version) throws IOException {
        File root = rootDirectory(context);
        File stamp = new File(root, VERSION_FILE);

        if (stamp.isFile() && version.equals(readText(stamp))) {
            Log.i(LOG_TAG, "PresetAssets up to date (version " + version + "): " + root);
            return root;
        }

        long started = SystemClock.elapsedRealtime();
        deleteRecursively(root);

        if (!root.mkdirs())
            throw new IOException("could not create " + root);

        AssetManager assets = context.getAssets();
        long[] bytes = new long[1];
        int presets = copyDirectory(assets, PRESETS_DIRECTORY, new File(root, PRESETS_DIRECTORY), PRESET_EXTENSION, bytes);
        int textures = copyDirectory(assets, TEXTURES_DIRECTORY, new File(root, TEXTURES_DIRECTORY), null, bytes);
        int audio = copyDirectory(assets, AUDIO_DIRECTORY, new File(root, AUDIO_DIRECTORY), null, bytes);
        writeText(stamp, version);

        Log.i(LOG_TAG, String.format(Locale.ROOT, "PresetAssets copied %d presets, %d textures, %d audio files (%d bytes) in %d ms (version %s): %s",
                presets, textures, audio, bytes[0], SystemClock.elapsedRealtime() - started, version, root));
        return root;
    }

    public static File rootDirectory(Context context) {
        return new File(context.getFilesDir(), ROOT_DIRECTORY);
    }

    public static File presetsDirectory(File root) {
        return new File(root, PRESETS_DIRECTORY);
    }

    public static File texturesDirectory(File root) {
        return new File(root, TEXTURES_DIRECTORY);
    }

    public static File audioDirectory(File root) {
        return new File(root, AUDIO_DIRECTORY);
    }

    private static String packageVersion(Context context) throws IOException {
        try {
            return Long.toString(context.getPackageManager().getPackageInfo(context.getPackageName(), 0).lastUpdateTime);
        } catch (PackageManager.NameNotFoundException error) {
            throw new IOException("package info unavailable for " + context.getPackageName(), error);
        }
    }

    private static int copyDirectory(AssetManager assets, String assetDirectory, File destination, String extension, long[] bytes) throws IOException {
        String[] names = assets.list(assetDirectory);

        if (names == null || names.length == 0)
            return 0;

        if (!destination.isDirectory() && !destination.mkdirs())
            throw new IOException("could not create " + destination);

        int copied = 0;
        byte[] buffer = new byte[COPY_BUFFER_BYTES];

        for (String name : names) {
            if (extension != null && !name.toLowerCase(Locale.ROOT).endsWith(extension))
                continue;

            String assetPath = assetDirectory + "/" + name;

            try (InputStream input = assets.open(assetPath);
                 OutputStream output = new FileOutputStream(new File(destination, name))) {
                int read;

                while ((read = input.read(buffer)) > 0) {
                    output.write(buffer, 0, read);
                    bytes[0] += read;
                }
            } catch (IOException error) {
                if (assets.list(assetPath).length > 0)
                    continue;
                throw error;
            }

            ++copied;
        }

        return copied;
    }

    private static void deleteRecursively(File file) throws IOException {
        File[] children = file.listFiles();

        if (children != null) {
            for (File child : children)
                deleteRecursively(child);
        }

        if (file.exists() && !file.delete())
            throw new IOException("could not delete " + file);
    }

    private static String readText(File file) throws IOException {
        return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
    }

    private static void writeText(File file, String text) throws IOException {
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write(text.getBytes(StandardCharsets.UTF_8));
        }
    }
}
