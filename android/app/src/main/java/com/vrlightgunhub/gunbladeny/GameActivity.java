// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.gunbladeny;
import org.libsdl.app.SDLActivity;
import android.os.Environment;
import android.content.Intent;
import android.os.Build;
import android.Manifest;
import android.content.pm.PackageManager;
import java.io.File;

public final class GameActivity extends SDLActivity {
    @Override protected void onNativeMainReturned() {
        Intent setup=new Intent(Intent.ACTION_MAIN).setClass(this,SetupActivity.class).putExtra("returned",true);
        setup.addCategory("com.oculus.intent.category.2D");
        setup.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(setup); finishAndRemoveTask();
    }
    @Override protected String[] getLibraries() { return new String[]{"SDL2","main"}; }
    @Override protected String[] getArguments() {
        String root=getFilesDir().getAbsolutePath();
        File download=Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        File external=getExternalFilesDir(null);
        File owned=external!=null?external:getFilesDir();
        boolean downloadsAllowed=Build.VERSION.SDK_INT>=30?Environment.isExternalStorageManager():
            checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)==PackageManager.PERMISSION_GRANTED;
        File games=GameStorage.romDirectory(owned,download,downloadsAllowed);
        nativeSetenv("GUNBLADE_VR","1");
        // Paired native RAM and framebuffer proofs verify this replaces only
        // duplicate software raster work. Original service/fallback paths stay native.
        nativeSetenv("GUNBLADE_FAST_RASTER","1");
        // Paired ARM64 native audio, RAM and framebuffer checks preserve the
        // original clock phase and CPU scheduling while batching quiet edges.
        nativeSetenv("GUNBLADE_UART_COALESCE","1");
        nativeSetenv("GUNBLADE_XR_LIBRARY",getApplicationInfo().nativeLibraryDir+"/libgunblade_xr.so");
        // The worker adapts playback to measured native production without
        // changing the arcade clocks, synthesized audio, or reference capture.
        nativeSetenv("GUNBLADE_AUDIO_LIBRARY",getApplicationInfo().nativeLibraryDir+"/libgunblade_audio.so");
        nativeSetenv("GUNBLADE_AUDIO_STRETCH","1");
        nativeSetenv("GUNBLADE_ASSETS",root+"/assets");
        nativeSetenv("GUNBLADE_SETTINGS",root+"/settings.ini");
        File logs=getExternalFilesDir(null);
        nativeSetenv("GUNBLADE_XR_LOG",new File(logs!=null?logs:getFilesDir(),"gunblade-xr.log").getAbsolutePath());
        nativeSetenv("GUNBLADE_AUDIO_LOG",new File(logs!=null?logs:getFilesDir(),"gunblade-audio.tsv").getAbsolutePath());
        File diagnosticRoot=logs!=null?logs:getFilesDir();
        nativeSetenv("GUNBLADE_FOVEATION",new File(diagnosticRoot,"foveation.off").isFile()?"0":"1");
        nativeSetenv("GUNBLADE_RETICLE_DIAGNOSTICS",new File(diagnosticRoot,"reticle-diagnostics.on").isFile()?
                new File(diagnosticRoot,"reticle-diagnostics").getAbsolutePath():"");
        nativeSetenv("GUNBLADE_CUSTOM_GUN",GameStorage.customGun(owned,download,downloadsAllowed).getAbsolutePath());
        return new String[]{"gunblade","-rompath",games.getAbsolutePath(),
            "-nvram_directory",root+"/nvram","-cfg_directory",root+"/cfg","-state_directory",root+"/state","-snapshot_directory",root+"/snap",
            "-video","none","-sound","sdl","-skip_gameinfo","-noreadconfig","-nomouse","-numprocessors","2","-throttle"};
    }
}
