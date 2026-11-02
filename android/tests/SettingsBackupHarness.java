// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.gunbladeny;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Real filesystem checks: the copier must never modify private inputs or older exports. */
public final class SettingsBackupHarness {
    private static void require(boolean value,String message) {
        if(!value) throw new AssertionError(message);
    }
    private static Path directory(Path parent,String name) throws IOException {
        return Files.createDirectories(parent.resolve(name));
    }
    private static void put(Path path,String value) throws IOException {
        Files.createDirectories(path.getParent());
        Files.write(path,value.getBytes(StandardCharsets.UTF_8));
    }
    private static String text(Path path) throws IOException {
        return new String(Files.readAllBytes(path),StandardCharsets.UTF_8);
    }
    private static Set<String> files(Path root) throws IOException {
        Set<String> names=new TreeSet<>();
        try(java.util.stream.Stream<Path> walk=Files.walk(root)) {
            walk.filter(Files::isRegularFile).forEach(path->names.add(root.relativize(path).toString().replace('\\','/')));
        }
        return names;
    }
    private static void failure(Path privateFiles,Path external,String reason) throws IOException {
        try {
            SettingsBackup.create(privateFiles.toFile(),external.toFile());
            throw new AssertionError("Accepted "+reason);
        } catch(IOException expected) { }
        try(DirectoryStream<Path> entries=Files.newDirectoryStream(external,"settings-backup-*")) {
            for(Path snapshot:entries) {
                require(text(snapshot.resolve("status.txt")).startsWith("state=failed\n"),"Failure marked complete: "+reason);
                require(!Files.exists(snapshot.resolve("manifest.json")),"Failure published complete manifest: "+reason);
            }
        }
    }
    public static void main(String[] args) throws Exception {
        Path base=Paths.get(args[0]);
        Path original=directory(base,"success/private"), external=directory(base,"success/external");
        put(original.resolve("settings.ini"),"world_scale=0.080\n");
        put(original.resolve("cfg/gunblade.cfg"),"operator=true\n");
        put(original.resolve("nvram/nested/scores.bin"),"scores\u0000\u00ff");
        directory(original,"cfg/empty");
        put(original.resolve("roms/gunblade.zip"),"private game media");
        put(original.resolve("state/resume.sta"),"unrelated state");
        put(original.resolve("unrelated.txt"),"untouched");
        Set<String> before=files(original);
        Path first=SettingsBackup.create(original.toFile(),external.toFile()).toPath();
        byte[] manifest=Files.readAllBytes(first.resolve("manifest.json"));
        Path second=SettingsBackup.create(original.toFile(),external.toFile()).toPath();
        require(!first.equals(second),"Snapshots collided");
        require(Arrays.equals(manifest,Files.readAllBytes(first.resolve("manifest.json"))),"Older snapshot changed");
        require(files(original).equals(before),"Private tree changed");
        require(text(original.resolve("settings.ini")).equals("world_scale=0.080\n"),"Settings changed");
        require(text(original.resolve("cfg/gunblade.cfg")).equals("operator=true\n"),"cfg changed");
        require(text(original.resolve("nvram/nested/scores.bin")).equals("scores\u0000\u00ff"),"nvram changed");
        require(files(first).equals(new TreeSet<>(Arrays.asList("settings.ini","cfg/gunblade.cfg",
                "nvram/nested/scores.bin","manifest.json","status.txt"))),"Allowlist leaked files");
        require(Files.isDirectory(first.resolve("cfg/empty")),"Empty native directory was lost");
        require(text(first.resolve("status.txt")).startsWith("state=complete\n"),"No verified completion");

        Path absent=directory(base,"absent/private"), absentExternal=directory(base,"absent/external");
        Path empty=SettingsBackup.create(absent.toFile(),absentExternal.toFile()).toPath();
        require(files(absent).isEmpty(),"Missing settings were created");
        require(files(empty).equals(new TreeSet<>(Arrays.asList("manifest.json","status.txt"))),"Absent roots were invented");
        require(text(empty.resolve("manifest.json")).contains("\"settings.ini\":\"missing\""),"Absence not recorded");

        Path oversized=directory(base,"oversized/private"), oversizedExternal=directory(base,"oversized/external");
        try(RandomAccessFile file=new RandomAccessFile(oversized.resolve("settings.ini").toFile(),"rw")) { file.setLength(4L*1024*1024+1); }
        failure(oversized,oversizedExternal,"oversized file");
        require(Files.size(oversized.resolve("settings.ini"))==4L*1024*1024+1,"Oversized original changed");
        Path total=directory(base,"total/private/cfg").getParent(), totalExternal=directory(base,"total/external");
        for(int i=0;i<5;i++) try(RandomAccessFile file=new RandomAccessFile(total.resolve("cfg/"+i).toFile(),"rw")) { file.setLength(4L*1024*1024); }
        failure(total,totalExternal,"total byte limit");
        Path wrong=directory(base,"wrong/private/settings.ini").getParent(), wrongExternal=directory(base,"wrong/external");
        failure(wrong,wrongExternal,"directory in settings.ini position");
        Path overlap=directory(base,"overlap/private");
        put(overlap.resolve("settings.ini"),"keep");
        try { SettingsBackup.create(overlap.toFile(),overlap.toFile()); throw new AssertionError("Accepted overlapping roots"); }
        catch(IOException expected) { }
        require(files(overlap).equals(Collections.singleton("settings.ini")),"Overlap wrote into original");
        Path collision=base.resolve("collision"); put(collision,"keep existing output");
        try { SettingsBackup.create(original.toFile(),collision.toFile()); throw new AssertionError("Overwrote output file"); }
        catch(IOException expected) { }
        require(text(collision).equals("keep existing output"),"Output collision changed existing file");

        Path links=directory(base,"links/private/cfg").getParent(), linksExternal=directory(base,"links/external");
        Path secret=base.resolve("links/secret.txt"); put(secret,"must remain private");
        try {
            Files.createSymbolicLink(links.resolve("cfg/escape"),secret.toAbsolutePath());
            failure(links,linksExternal,"symlink escape");
            require(text(secret).equals("must remain private"),"Symlink target changed");
        } catch(UnsupportedOperationException | FileSystemException unavailable) {
            System.out.println("Symlink fixture unavailable on this host: "+unavailable.getClass().getSimpleName());
        }
        // Windows junctions require no symlink privilege and still exercise canonical escape rejection.
        if(System.getProperty("os.name").startsWith("Windows")) {
            Path junctionPrivate=directory(base,"junction/private/cfg").getParent();
            Path junctionExternal=directory(base,"junction/external");
            Path target=directory(base,"junction/secret"); put(target.resolve("secret.txt"),"not exported");
            Process process=new ProcessBuilder("cmd.exe","/d","/c","mklink","/J",
                    junctionPrivate.resolve("cfg/escape").toString(),target.toString()).redirectErrorStream(true).start();
            String result=new String(process.getInputStream().readAllBytes(),StandardCharsets.UTF_8);
            if(process.waitFor()==0) {
                failure(junctionPrivate,junctionExternal,"junction path escape");
                require(text(target.resolve("secret.txt")).equals("not exported"),"Junction target changed");
                Path rootAlias=base.resolve("junction/private-alias");
                Process alias=new ProcessBuilder("cmd.exe","/d","/c","mklink","/J",rootAlias.toString(),
                        junctionPrivate.toString()).redirectErrorStream(true).start();
                String aliasResult=new String(alias.getInputStream().readAllBytes(),StandardCharsets.UTF_8);
                require(alias.waitFor()==0,"Cannot create root-overlap fixture: "+aliasResult);
                try {
                    SettingsBackup.create(junctionPrivate.toFile(),rootAlias.toFile());
                    throw new AssertionError("Accepted roots overlapping through a junction");
                } catch(IOException expected) { }
                try(DirectoryStream<Path> snapshots=Files.newDirectoryStream(junctionPrivate,"settings-backup-*")) {
                    require(!snapshots.iterator().hasNext(),"Root alias wrote a snapshot into private storage");
                }
            } else System.out.println("Junction fixture unavailable on this host: "+result.trim());
        }
    }
}
