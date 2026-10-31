package com.vrlightgunhub.gunbladeny;
import java.io.File;
import java.io.IOException;

/** Missing-payload help must refer to the selected location, including app-owned media. */
public final class SetupFilesHarness {
    public static void main(String[] args) throws Exception {
        File game = new File(args[0]);
        for (String name : new String[] {"game.rom", "epr-18022.ic2"}) {
            try {
                GameFiles.validateArchives(new File[] {game}, new GameFiles.Rom[] {
                    new GameFiles.Rom(name, 3, 0x352441c2L, "a9993e364706816aba3e25717850c26c9cd0d89d")
                });
                throw new AssertionError("missing payload was accepted");
            } catch (IOException error) {
                if (!error.getMessage().contains(game.getParentFile().getAbsolutePath()))
                    throw new AssertionError("help sends app-owned media to the wrong folder: "+error.getMessage());
                if (!error.getMessage().contains(name))
                    throw new AssertionError("help must identify the missing ROM");
            }
        }
        System.out.println("PASS selected app-folder ROM diagnostics");
    }
}
