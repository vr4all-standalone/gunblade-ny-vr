// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.gunbladeny;
import java.io.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

/** Validate original packed ROMs in place; never unpack game data into app state. */
public final class GameFiles {
    static final class Rom {
        final String name, sha1; final long size, crc;
        Rom(String name, long size, long crc, String sha1) {
            this.name=name; this.size=size; this.crc=crc; this.sha1=sha1;
        }
    }
    private static final Rom[] REQUIRED = {
        new Rom("epr-18988a.15", 524288, 0xf63f1ad2L, "fcfb0a4691cd7d66168c421e4e1694ecaea56ab2"),
        new Rom("epr-18989a.16", 524288, 0xc1c84d65L, "92bffbf1250c53499c37a53f9e2a054fc7bf256f"),
        new Rom("mpr-18974.11", 4194304, 0xe29ecaffL, "dcdfe9f59158cec2f02b213ee13f5e40cdb92e55"),
        new Rom("mpr-18975.12", 4194304, 0xd8187582L, "34a0b32eeed1a9f41bca8b9261851881b2ba79f2"),
        new Rom("mpr-18976.9", 4194304, 0xc95c15ebL, "892063e91b2ed20e0600d4b188da1e9f45a19692"),
        new Rom("mpr-18977.10", 4194304, 0xdb8f5b6fL, "c11d2c9e1e215aa7b2ebb777639c8cd651901f52"),
        new Rom("mpr-18986.29", 4194304, 0x04820f7bL, "5eb6682399b358d77658d82e612b02b724e3f3e1"),
        new Rom("mpr-18987.30", 4194304, 0x2419367fL, "0a04a1049d2da486dc9dbb97b383bd24259b78c8"),
        new Rom("mpr-18980.17", 4194304, 0x22345534L, "7b8bdcfe88953ce1b2d75af2ce4712ab6507e2cf"),
        new Rom("mpr-18981.21", 4194304, 0x2544a33dL, "a76193f70adb6abeba02328b290af5cca47d4e25"),
        new Rom("mpr-18982.18", 4194304, 0xd0a92b2aL, "95404baed88cc95b75ff9b9084d09622961d3e57"),
        new Rom("mpr-18983.22", 4194304, 0x1b4af982L, "550f8248699b9267da7d2e64002be56972381714"),
        new Rom("mpr-18985.27", 4194304, 0xad6166e3L, "2c487fb743730cacf92dbea952b1efada0f073df"),
        new Rom("mpr-18984.25", 4194304, 0x756f6f37L, "095964de773f515d64d65dbc8f8ef9bae97e5ba9"),
        new Rom("epr-18990.31", 524288, 0x02b1b0d1L, "759b4683dc7149e04f41ddac7bd395e8d07ea858"),
        new Rom("mpr-18978.32", 4194304, 0x0f78b3e3L, "6c2cd6236cb001bb8d487a9b1e9907519dc43daa"),
        new Rom("mpr-18979.34", 4194304, 0xf13ea36fL, "a8165116b5e07e031ff960201dd8c9a441544961"),
        new Rom("epr-18022.ic2", 65536, 0x0ca70f80L, "edf5ade72d9fa2f4d5f83f9f89e6cecfadd77f56"),
    };
    private static final long MAX_ARCHIVE_BYTES=128L*1024*1024;
    private static final int MAX_MEMBERS=4096;
    private GameFiles() {}

    public static void validate(File game, File billboard) throws IOException {
        if(game==null || !game.isFile()) throw new IOException("Missing game archive: "+(game==null?"gunblade.zip":game.getAbsolutePath()));
        File[] paths=billboard!=null && billboard.isFile()?new File[]{game,billboard}:new File[]{game};
        validateArchives(paths, REQUIRED);
    }

    static void validateArchives(File[] paths, Rom[] requirements) throws IOException {
        Map<String,Rom> wanted=new HashMap<>();
        for(Rom rom:requirements) wanted.put(rom.name,rom);
        Set<String> found=new HashSet<>();
        for(File path:paths) {
            if(!path.isFile() || !path.canRead()) throw new IOException("Cannot read "+path.getName()+". Allow game file access and retry.");
            if(path.length()>MAX_ARCHIVE_BYTES) throw new IOException("ROM archive is too large: "+path.getName());
            try(ZipFile zip=new ZipFile(path)) {
                if(zip.size()>MAX_MEMBERS) throw new IOException("Too many ZIP members in "+path.getName());
                Set<String> seen=new HashSet<>();
                Enumeration<? extends ZipEntry> entries=zip.entries();
                while(entries.hasMoreElements()) {
                    ZipEntry entry=entries.nextElement(); String name=entry.getName();
                    if(name.isEmpty() || name.equals(".") || name.equals("..") || name.indexOf('/')>=0 || name.indexOf('\\')>=0 || name.indexOf(':')>=0 || name.indexOf('\0')>=0)
                        throw new IOException("Unsafe ZIP member: "+name);
                    String key=name.toLowerCase(Locale.ROOT);
                    if(!seen.add(key)) throw new IOException("Duplicate ZIP member: "+name);
                    Rom rom=wanted.get(key);
                    if(rom==null) continue;
                    if(entry.isDirectory() || entry.getSize()!=rom.size) throw new IOException("Wrong ROM size: "+rom.name);
                    MessageDigest sha;
                    try { sha=MessageDigest.getInstance("SHA-1"); }
                    catch(NoSuchAlgorithmException error) { throw new IOException("SHA-1 validation is unavailable",error); }
                    CRC32 crc=new CRC32(); byte[] buffer=new byte[16384]; long count=0;
                    try(InputStream in=zip.getInputStream(entry)) {
                        int bytes;
                        while((bytes=in.read(buffer,0,(int)Math.min(buffer.length,rom.size-count+1)))!=-1) {
                            if(bytes==0) continue;
                            count+=bytes;
                            if(count>rom.size) throw new IOException("Oversized ROM payload: "+rom.name);
                            sha.update(buffer,0,bytes); crc.update(buffer,0,bytes);
                        }
                    }
                    if(count!=rom.size || crc.getValue()!=rom.crc || !hex(sha.digest()).equals(rom.sha1)) throw new IOException("ROM checksum mismatch: "+rom.name+". Copy a complete, unmodified archive.");
                    found.add(key);
                }
            } catch(ZipException error) {
                throw new IOException("Cannot read "+path.getName()+": invalid, corrupt or encrypted ZIP ("+error.getMessage()+").",error);
            }
        }
        for(Rom rom:requirements) if(!found.contains(rom.name)) {
            File directory=paths[0].getAbsoluteFile().getParentFile();
            String source=rom.name.equals("epr-18022.ic2")?
                    new File(directory,"segabill.zip")+" (split-set billboard firmware), or a complete gunblade.zip":
                    new File(directory,"gunblade.zip").getAbsolutePath();
            throw new IOException("Missing ROM: "+rom.name+". Required in "+source+".");
        }
    }
    private static String hex(byte[] bytes) {
        char[] symbols="0123456789abcdef".toCharArray(); char[] result=new char[bytes.length*2];
        for(int i=0;i<bytes.length;i++) { result[i*2]=symbols[(bytes[i]&255)>>>4]; result[i*2+1]=symbols[bytes[i]&15]; }
        return new String(result);
    }
}
