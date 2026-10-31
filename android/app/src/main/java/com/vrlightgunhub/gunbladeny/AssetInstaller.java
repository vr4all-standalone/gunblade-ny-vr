// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.gunbladeny;

import java.io.*;
import java.nio.file.*;

/** Publish complete assets even when workers from recreated activities overlap. */
final class AssetInstaller {
    private AssetInstaller() {}

    /** The caller owns input. A failed copy/atomic move preserves the old destination. */
    static void install(InputStream input,File output) throws IOException {
        Path destination=output.toPath().toAbsolutePath(), directory=destination.getParent();
        Files.createDirectories(directory);
        Path temporary=Files.createTempFile(directory,".vr4all-asset-",".tmp");
        try {
            try(OutputStream file=Files.newOutputStream(temporary)) {
                byte[] buffer=new byte[16384]; int bytes;
                while((bytes=input.read(buffer))!=-1) file.write(buffer,0,bytes);
            }
            // Same-directory atomic rename: readers see either complete file.
            // If atomic moves are unavailable, fail without a truncating fallback.
            Files.move(temporary,destination,StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
        } finally {
            Files.deleteIfExists(temporary);
        }
    }
}
