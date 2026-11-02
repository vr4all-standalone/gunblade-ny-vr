// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.gunbladeny;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.security.*;
import java.util.*;

/** Opt-in, read-only export of native settings. Never loads the engine or creates private state. */
final class SettingsBackup {
    private static final String[] ROOTS={"settings.ini","cfg","nvram"};
    private static final long FILE_LIMIT=4L*1024*1024, TOTAL_LIMIT=16L*1024*1024;
    private static final int ENTRY_LIMIT=1024, DEPTH_LIMIT=32;
    private SettingsBackup() { }

    /** Each invocation gets a fresh directory; a missing status or manifest means incomplete. */
    static File create(File privateFiles,File externalFiles) throws IOException {
        Path source=root(privateFiles), external=root(externalFiles);
        if(source.startsWith(external) || external.startsWith(source))
            throw new IOException("Private and export directories overlap");
        Path snapshot=child(external,"settings-backup-"+System.currentTimeMillis()+"-"+UUID.randomUUID());
        // CREATE_NEW semantics: never merge with or replace any existing export.
        Files.createDirectory(snapshot);
        try {
            Inventory inventory=new Inventory(source);
            for(String name:ROOTS) {
                Path entry=source.resolve(name);
                if(!Files.exists(entry,LinkOption.NOFOLLOW_LINKS)) {
                    inventory.roots.put(name,"missing");
                    continue;
                }
                inventory.roots.put(name,"present");
                inventory.inspect(entry,name,0,name.equals("settings.ini")?1:2);
            }
            for(String relative:inventory.directories) {
                Path output=child(snapshot,relative);
                Files.createDirectory(output);
            }
            for(Entry entry:inventory.files) {
                Path input=child(source,entry.path), output=child(snapshot,entry.path);
                entry.originalHash=copy(input,output,entry.length);
                entry.copiedHash=hash(child(snapshot,entry.path),entry.length);
                String originalAgain=hash(child(source,entry.path),entry.length);
                if(!entry.originalHash.equals(entry.copiedHash) || !entry.originalHash.equals(originalAgain))
                    throw new IOException("Settings changed or copy verification failed: "+entry.path);
            }
            // Publish manifest only after all original and copied lengths/hashes agree.
            writeNew(child(snapshot,"manifest.json"),inventory.manifest());
            writeNew(child(snapshot,"status.txt"),"state=complete\nfiles="+inventory.files.size()+
                    "\nbytes="+inventory.total+"\nmanifest=manifest.json\n");
            return snapshot.toFile();
        } catch(IOException | RuntimeException failure) {
            try { writeNew(child(snapshot,"status.txt"),"state=failed\nreason="+
                    String.valueOf(failure.getMessage()).replace('\n',' ').replace('\r',' ')+"\n"); }
            catch(IOException statusFailure) { failure.addSuppressed(statusFailure); }
            throw failure;
        }
    }

    private static Path root(File directory) throws IOException {
        if(directory==null) throw new IOException("App directory unavailable");
        Path supplied=directory.toPath().toAbsolutePath().normalize();
        if(Files.isSymbolicLink(supplied)) throw new IOException("Symlink app directory rejected");
        Path result=supplied.toRealPath();
        if(!Files.isDirectory(result,LinkOption.NOFOLLOW_LINKS)) throw new IOException("App directory unavailable");
        return result;
    }

    /** Resolve only inspected relative entries, rejecting symlinks including in-root aliases. */
    private static Path child(Path root,String relative) throws IOException {
        Path path=root.resolve(relative).normalize();
        if(path.equals(root) || !path.startsWith(root) || !root.toRealPath().equals(root))
            throw new IOException("Settings path escape rejected: "+relative);
        Path current=root;
        for(Path component:root.relativize(path)) {
            current=current.resolve(component);
            if(Files.isSymbolicLink(current)) throw new IOException("Settings symlink rejected: "+relative);
            // getCanonicalFile does not resolve Windows junctions. Resolve every existing
            // parent with the provider; only a not-yet-created output suffix may be absent.
            if(Files.exists(current,LinkOption.NOFOLLOW_LINKS) && !current.toRealPath().equals(current))
                throw new IOException("Settings real-path escape rejected: "+relative);
        }
        return path;
    }

    private static final class Entry {
        final String path;
        final long length;
        String originalHash,copiedHash;
        Entry(String path,long length) { this.path=path; this.length=length; }
    }

    private static final class Inventory {
        final Path source;
        final Map<String,String> roots=new LinkedHashMap<>();
        final List<String> directories=new ArrayList<>();
        final List<Entry> files=new ArrayList<>();
        long total;
        int entries;
        Inventory(Path source) { this.source=source; }
        void inspect(Path path,String relative,int depth,int requiredType) throws IOException {
            path=child(source,relative);
            if(++entries>ENTRY_LIMIT || depth>DEPTH_LIMIT) throw new IOException("Settings entry/depth limit exceeded");
            BasicFileAttributes attributes=Files.readAttributes(path,BasicFileAttributes.class,LinkOption.NOFOLLOW_LINKS);
            if(attributes.isSymbolicLink() || (!attributes.isDirectory() && !attributes.isRegularFile()))
                throw new IOException("Unsupported settings entry: "+relative);
            if((requiredType==1 && !attributes.isRegularFile()) || (requiredType==2 && !attributes.isDirectory()))
                throw new IOException("Unexpected settings entry type: "+relative);
            if(attributes.isDirectory()) {
                directories.add(relative);
                List<Path> children=new ArrayList<>();
                try(DirectoryStream<Path> listing=Files.newDirectoryStream(path)) {
                    for(Path item:listing) {
                        if(children.size()>=ENTRY_LIMIT) throw new IOException("Settings entry limit exceeded");
                        children.add(item);
                    }
                }
                Collections.sort(children);
                for(Path item:children) inspect(item,relative+"/"+item.getFileName(),depth+1,0);
            } else {
                long size=attributes.size();
                if(size>FILE_LIMIT || size>TOTAL_LIMIT-total) throw new IOException("Settings byte limit exceeded: "+relative);
                total+=size;
                files.add(new Entry(relative,size));
            }
        }
        String manifest() {
            StringBuilder json=new StringBuilder("{\n\"schemaVersion\":1,\n\"complete\":true,\n\"roots\":{");
            boolean comma=false;
            for(Map.Entry<String,String> root:roots.entrySet()) {
                if(comma) json.append(','); comma=true;
                json.append(quote(root.getKey())).append(':').append(quote(root.getValue()));
            }
            json.append("},\n\"directories\":["); comma=false;
            for(String directory:directories) {
                if(comma) json.append(','); comma=true; json.append(quote(directory));
            }
            json.append("],\n\"files\":[\n"); comma=false;
            for(Entry entry:files) {
                if(comma) json.append(",\n"); comma=true;
                json.append("{\"path\":").append(quote(entry.path))
                    .append(",\"originalLength\":").append(entry.length)
                    .append(",\"copiedLength\":").append(entry.length)
                    .append(",\"originalSha256\":").append(quote(entry.originalHash))
                    .append(",\"copiedSha256\":").append(quote(entry.copiedHash)).append('}');
            }
            return json.append("\n]}\n").toString();
        }
    }

    private static String copy(Path input,Path output,long expected) throws IOException {
        try(InputStream from=Files.newInputStream(input,StandardOpenOption.READ,LinkOption.NOFOLLOW_LINKS);
            OutputStream to=Files.newOutputStream(output,StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE,LinkOption.NOFOLLOW_LINKS)) {
            return transfer(from,to,expected);
        }
    }
    private static String hash(Path input,long expected) throws IOException {
        try(InputStream from=Files.newInputStream(input,StandardOpenOption.READ,LinkOption.NOFOLLOW_LINKS)) {
            return transfer(from,null,expected);
        }
    }
    private static String transfer(InputStream from,OutputStream to,long expected) throws IOException {
        MessageDigest digest;
        try { digest=MessageDigest.getInstance("SHA-256"); }
        catch(NoSuchAlgorithmException unavailable) { throw new IOException("SHA-256 unavailable",unavailable); }
        byte[] buffer=new byte[16384]; long length=0; int count;
        while((count=from.read(buffer))!=-1) {
            length+=count;
            if(length>expected || length>FILE_LIMIT) throw new IOException("Settings size changed during backup");
            digest.update(buffer,0,count);
            if(to!=null) to.write(buffer,0,count);
        }
        if(length!=expected) throw new IOException("Settings size changed during backup");
        StringBuilder hex=new StringBuilder(64);
        for(byte value:digest.digest()) hex.append(String.format(Locale.ROOT,"%02x",value&255));
        return hex.toString();
    }
    private static void writeNew(Path path,String text) throws IOException {
        try(OutputStream output=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE,LinkOption.NOFOLLOW_LINKS)) {
            output.write(text.getBytes(StandardCharsets.UTF_8));
        }
    }
    private static String quote(String text) {
        StringBuilder json=new StringBuilder("\"");
        for(int i=0;i<text.length();i++) {
            char c=text.charAt(i);
            if(c=='"' || c=='\\') json.append('\\').append(c);
            else if(c<32) json.append(String.format(Locale.ROOT,"\\u%04x",(int)c));
            else json.append(c);
        }
        return json.append('"').toString();
    }
}
