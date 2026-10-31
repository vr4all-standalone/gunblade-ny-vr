package com.vrlightgunhub.gunbladeny;

import java.io.*;
import java.nio.file.*;
import java.util.Arrays;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicReference;

/** Real files, failed reads and a deterministic resize-worker interleaving. */
public final class AssetInstallerHarness {
    private static byte[] payload(int size, int value) {
        byte[] data=new byte[size]; Arrays.fill(data,(byte)value); return data;
    }
    private static void expect(boolean value,String message) {
        if(!value) throw new AssertionError(message);
    }
    private static void contents(Path file,byte[] wanted,String message) throws IOException {
        expect(Arrays.equals(Files.readAllBytes(file),wanted),message);
    }
    private static void noTemps(Path folder,int expected) throws IOException {
        try(java.util.stream.Stream<Path> entries=Files.list(folder)) {
            expect(entries.count()==expected,"failed/successful install left temporary assets behind");
        }
    }
    private static final class BrokenInput extends InputStream {
        private boolean supplied;
        @Override public int read() throws IOException { throw new IOException("synthetic read failure"); }
        @Override public int read(byte[] out,int offset,int count) throws IOException {
            if(supplied) throw new IOException("synthetic read failure");
            supplied=true; Arrays.fill(out,offset,offset+Math.min(count,4096),(byte)17);
            return Math.min(count,4096);
        }
    }
    private static final class GatedInput extends ByteArrayInputStream {
        final CountDownLatch staged=new CountDownLatch(1), release=new CountDownLatch(1);
        private boolean supplied;
        GatedInput(byte[] data) { super(data); }
        @Override public synchronized int read(byte[] out,int offset,int count) {
            if(!supplied) { supplied=true; return super.read(out,offset,Math.min(count,4096)); }
            staged.countDown();
            try {
                if(!release.await(10,TimeUnit.SECONDS)) throw new AssertionError("worker was not released");
            } catch(InterruptedException error) { throw new AssertionError(error); }
            return super.read(out,offset,count);
        }
        void awaitStaged() throws InterruptedException {
            expect(staged.await(10,TimeUnit.SECONDS),"worker did not reach a partially written copy");
        }
    }
    private static Thread worker(GatedInput input,Path file,AtomicReference<Throwable> failure) {
        Thread thread=new Thread(()->{
            try { AssetInstaller.install(input,file.toFile()); }
            catch(Throwable error) { failure.compareAndSet(null,error); }
        });
        thread.start(); return thread;
    }
    private static void joined(Thread thread,AtomicReference<Throwable> failure) throws Exception {
        thread.join(10000);
        expect(!thread.isAlive(),"asset worker did not finish");
        if(failure.get()!=null) throw new AssertionError("asset worker failed",failure.get());
    }
    public static void main(String[] args) throws Exception {
        Path root=Paths.get(args[0]), failed=Files.createDirectory(root.resolve("failed"));
        Path existing=failed.resolve("font.ttf"); byte[] original=payload(65536,1);
        Files.write(existing,original);
        for(Path target:new Path[]{existing,failed.resolve("new-model.glb")}) {
            try {
                AssetInstaller.install(new BrokenInput(),target.toFile());
                throw new AssertionError("failed input was accepted");
            } catch(IOException expected) {
                expect(expected.getMessage().contains("synthetic read failure"),"unexpected failure");
            }
            contents(existing,original,"failed copy changed the previous complete font");
            expect(!Files.exists(failed.resolve("new-model.glb")),"failed new copy exposed a partial asset");
            noTemps(failed,1);
        }

        Path overlap=Files.createDirectory(root.resolve("overlap")), asset=overlap.resolve("model.glb");
        Files.write(asset,original);
        byte[] older=payload(131072,2), newer=payload(262144,3);
        GatedInput first=new GatedInput(older), second=new GatedInput(newer);
        AtomicReference<Throwable> failure=new AtomicReference<>();
        Thread oldWorker=worker(first,asset,failure), newWorker=null;
        try {
            first.awaitStaged();
            contents(asset,original,"reader saw a partial asset from the older resize worker");
            newWorker=worker(second,asset,failure); second.awaitStaged();
            contents(asset,original,"reader saw a partial asset while both workers copied");
            second.release.countDown(); joined(newWorker,failure);
            contents(asset,newer,"new worker did not publish a complete asset");
            contents(asset,newer,"older in-flight copy changed the newly published asset");
            first.release.countDown(); joined(oldWorker,failure);
            contents(asset,older,"older worker did not publish a complete replacement");
            noTemps(overlap,1);
        } finally {
            first.release.countDown(); second.release.countDown();
            oldWorker.join(10000); if(newWorker!=null) newWorker.join(10000);
        }

        Path live=Files.createDirectory(root.resolve("live-reader")), font=live.resolve("font.ttf");
        Files.write(font,original);
        boolean replaced=false;
        try(InputStream reader=Files.newInputStream(font)) {
            try {
                AssetInstaller.install(new ByteArrayInputStream(newer),font.toFile()); replaced=true;
            } catch(IOException safeRefusal) {
                // Some desktop filesystems prohibit replacing a reader's open file.
                // Refusal must retain the old file rather than falling back to truncation.
            }
            expect(Arrays.equals(reader.readAllBytes(),original),"open reader saw replacement/partial bytes");
        }
        contents(font,replaced?newer:original,"replacement did not leave one complete asset");
        noTemps(live,1);

        Path rejected=Files.createDirectory(root.resolve("rejected"));
        Path directory=Files.createDirectory(rejected.resolve("font.ttf"));
        Files.write(directory.resolve("preserve"),original);
        try {
            AssetInstaller.install(new ByteArrayInputStream(newer),directory.toFile());
            throw new AssertionError("invalid replacement was accepted");
        } catch(IOException expected) { }
        contents(directory.resolve("preserve"),original,"failed commit changed the prior destination");
        noTemps(rejected,1);
        System.out.println("PASS failed copies, atomic replacements, overlapping workers and temporary cleanup");
    }
}
