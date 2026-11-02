// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Entire implementation is absent from normal builds. There is no C ABI or
// user setting for capture; the diagnostic build also needs a one-use marker.
#if defined(GUNBLADE_AUDIO_DIAGNOSTICS) && GUNBLADE_AUDIO_DIAGNOSTICS
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <initializer_list>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace gunblade_audio_detail {
// One atomic closes admission and counts already-admitted copies. No callback
// retries, waits or locks. After close, a late entrant cannot touch the buffers.
class CaptureGate {
    static constexpr uint64_t Closed=uint64_t(1)<<63;
    std::atomic<uint64_t> value_{0};
    bool admit() noexcept {
        if(value_.fetch_add(1,std::memory_order_acq_rel)&Closed) {
            value_.fetch_sub(1,std::memory_order_release);return false;
        }
        return true;
    }
public:
    bool enter() noexcept {
        if(value_.load(std::memory_order_relaxed)&Closed)return false;
        return admit();
    }
    void leave() noexcept {value_.fetch_sub(1,std::memory_order_release);}
    void close() noexcept {value_.fetch_or(Closed,std::memory_order_acq_rel);}
    bool quiescent() const noexcept {return value_.load(std::memory_order_acquire)==Closed;}
#ifdef GUNBLADE_AUDIO_CAPTURE_TEST
    // Exercise the same admission step when close races the earlier fast probe.
    bool late_entry_for_test() noexcept {return admit();}
#endif
};

enum CaptureFlags : uint32_t {
    CaptureRunning=1,CaptureWarming=2,CaptureInvalidTime=4,
    CaptureHasInterval=8,CaptureRunningInterval=16,CapturePaused=32,
    CaptureClockReversal=64
};
struct CaptureEvent {
    double wall_s=0;
    uint64_t offset_frames=0,epoch=0;
    uint32_t requested=0,stored=0,accepted=0,delivered=0,missing=0,warmup=0,paused=0,flags=0;
    double interval_s=0;
};
static_assert(sizeof(CaptureEvent)==64);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::endian::native==std::endian::little);

class AudioCaptureBuffer {
    friend class AudioCapture;
    struct Stream {
        std::unique_ptr<int16_t[]> pcm;
        std::unique_ptr<CaptureEvent[]> events;
        uint64_t requested=0,capped_frames=0,dropped_events=0;
        uint32_t frames=0,event_count=0;
        bool have_previous=false;
        double previous_wall=0,max_unpaused_gap=0,max_running_gap=0;
        uint64_t previous_epoch=0,unpaused_intervals=0,running_intervals=0;
        uint32_t previous_flags=0;
    };
    CaptureGate gate_;
    const uint32_t frame_capacity_,event_capacity_;
    Stream source_,returned_;
    AudioCaptureBuffer(uint32_t frames,uint32_t events):frame_capacity_(frames),event_capacity_(events) {
        // Allocate and initialize full bounded arrays before audio callbacks.
        source_.pcm=std::make_unique<int16_t[]>(size_t(frames)*2);
        returned_.pcm=std::make_unique<int16_t[]>(size_t(frames)*2);
        source_.events=std::make_unique<CaptureEvent[]>(events);
        returned_.events=std::make_unique<CaptureEvent[]>(events);
    }
    void record(Stream& stream,const int16_t* data,uint32_t frames,double now,uint64_t control,
                uint32_t accepted,uint32_t delivered,uint32_t warmup,bool running) noexcept {
        if(!data || !frames || !gate_.enter())return;
        const uint32_t stored=std::min(frames,frame_capacity_-stream.frames);
        const bool paused=(control&1)!=0;
        CaptureEvent event;event.wall_s=now;event.offset_frames=stream.frames;event.epoch=control>>1;
        event.requested=frames;event.stored=stored;event.accepted=accepted;event.delivered=delivered;
        event.paused=paused?frames:0;event.missing=(&stream==&returned_ && !paused)?frames-delivered:0;
        event.warmup=warmup;event.flags=(running?CaptureRunning:0)|(warmup?CaptureWarming:0)|(paused?CapturePaused:0);
        if(std::isfinite(now)) {
            if(stream.have_previous && stream.previous_epoch==event.epoch) {
                if(now>=stream.previous_wall) {
                    event.flags|=CaptureHasInterval;event.interval_s=now-stream.previous_wall;
                    if(!paused && !(stream.previous_flags&CapturePaused)) {
                        ++stream.unpaused_intervals;stream.max_unpaused_gap=std::max(stream.max_unpaused_gap,event.interval_s);
                        if(running && (stream.previous_flags&CaptureRunning)) {
                            event.flags|=CaptureRunningInterval;++stream.running_intervals;
                            stream.max_running_gap=std::max(stream.max_running_gap,event.interval_s);
                        }
                    }
                } else event.flags|=CaptureClockReversal;
            }
            stream.have_previous=true;stream.previous_wall=now;stream.previous_epoch=event.epoch;stream.previous_flags=event.flags;
        } else {event.flags|=CaptureInvalidTime;stream.have_previous=false;}
        if(stored)std::memcpy(stream.pcm.get()+size_t(stream.frames)*2,data,size_t(stored)*4);
        stream.frames+=stored;stream.requested+=frames;stream.capped_frames+=frames-stored;
        if(stream.event_count<event_capacity_)stream.events[stream.event_count++]=event;
        else ++stream.dropped_events;
        gate_.leave();
    }
public:
    static constexpr uint32_t FrameCapacity=60*48000,EventCapacity=8192;
    AudioCaptureBuffer():AudioCaptureBuffer(FrameCapacity,EventCapacity) {}
    void push(const int16_t* data,uint32_t frames,double now,uint64_t control,uint32_t accepted) noexcept {
        record(source_,data,frames,now,control,accepted,0,0,false);
    }
    void read(const int16_t* data,uint32_t frames,double now,uint64_t control,
              uint32_t delivered,uint32_t warmup,bool running) noexcept {
        record(returned_,data,frames,now,control,0,delivered,warmup,running);
    }
    void freeze() noexcept {gate_.close();}
    bool quiescent() const noexcept {return gate_.quiescent();}
#ifdef GUNBLADE_AUDIO_CAPTURE_TEST
    static std::unique_ptr<AudioCaptureBuffer> for_test(uint32_t frames,uint32_t events) {
        return std::unique_ptr<AudioCaptureBuffer>(new AudioCaptureBuffer(frames,events));
    }
    uint32_t source_frames_for_test() const {return source_.frames;}
    uint32_t returned_frames_for_test() const {return returned_.frames;}
    uint64_t source_capped_for_test() const {return source_.capped_frames;}
    uint64_t read_events_dropped_for_test() const {return returned_.dropped_events;}
#endif
};

// Everything below is control-thread-only: marker claim, file creation,
// serialization and finalization. The DSP worker never writes capture files.
class AudioCapture {
    using Clock=std::chrono::steady_clock;
    std::filesystem::path directory_;
    AudioCaptureBuffer buffer_;
    const double seconds_;
    const Clock::time_point started_=Clock::now();
    std::atomic<bool> stopping_{false};
    std::thread writer_;
    struct WriteResult {bool ok=false;uint64_t bytes=0;};
    struct FilePart {const void* data;size_t size;};
    static bool capture_directory_name(std::string_view name) noexcept {
        constexpr std::string_view prefix="audio-capture-";
        if(!name.starts_with(prefix))return false;
        size_t at=prefix.size(),digits=at;
        while(at<name.size() && name[at]>='0' && name[at]<='9')++at;
        if(at==digits || at==name.size() || name[at++]!='-')return false;
        digits=at;while(at<name.size() && name[at]>='0' && name[at]<='9')++at;
        return at>digits && at==name.size();
    }
    static void repair_prior_capture_permissions(const std::filesystem::path& parent) noexcept {
#ifndef _WIN32
        struct Descriptor {int value;~Descriptor(){if(value>=0)::close(value);}};
        try {
            Descriptor base{::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
            if(base.value<0)return;
            std::error_code error;
            std::filesystem::directory_iterator entry(parent,error),end;
            while(!error && entry!=end) {
                const auto name=entry->path().filename().string();
                if(capture_directory_name(name)) {
                    // Directory-relative no-follow opens keep permission repair
                    // anchored to this app-owned parent, even across renames.
                    Descriptor directory{::openat(base.value,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
                    if(directory.value>=0)for(const char* file:{"request.on","started.json","source.wav","returned.wav","push.tsv","read.tsv","manifest.json","export-error.txt"}) {
                        Descriptor output{::openat(directory.value,file,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC)};
                        struct stat status{};
                        if(output.value>=0 && ::fstat(output.value,&status)==0 && S_ISREG(status.st_mode) && status.st_nlink==1)::fchmod(output.value,0644);
                    }
                }
                entry.increment(error);
            }
        } catch(...) {} // Diagnostic permission repair must never stop audio.
#else
        (void)parent;
#endif
    }
    static WriteResult exclusive_parts(const std::filesystem::path& path,std::initializer_list<FilePart> parts) noexcept {
#ifdef _WIN32
        const int fd=::_wopen(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
#else
        const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
        // An application umask must not make a diagnostic export unreadable.
        if(fd>=0 && ::fchmod(fd,0644)!=0){::close(fd);return {};}
#endif
        if(fd<0)return {};
        WriteResult result;uint64_t wanted=0;for(const auto& part:parts)wanted+=part.size;
        bool failed=false;
        for(const auto& part:parts) {
            const auto* bytes=static_cast<const uint8_t*>(part.data);size_t offset=0;
            while(offset<part.size) {
                const unsigned count=unsigned(std::min(size_t(1<<20),part.size-offset));
#ifdef _WIN32
                const int wrote=::_write(fd,bytes+offset,count);
#else
                const ssize_t wrote=::write(fd,bytes+offset,count);
#endif
                if(wrote<0 && errno==EINTR)continue;
                if(wrote<=0){failed=true;break;}
                offset+=size_t(wrote);result.bytes+=uint64_t(wrote);
            }
            if(failed)break;
        }
#ifdef _WIN32
        const int closed=::_close(fd);
#else
        const int closed=::close(fd);
#endif
        result.ok=result.bytes==wanted && closed==0;return result;
    }
    static WriteResult exclusive_file(const std::filesystem::path& path,const void* data,size_t size) noexcept {
        return exclusive_parts(path,{{data,size}});
    }
    static WriteResult text_file(const std::filesystem::path& path,std::string_view text) noexcept {
        return exclusive_file(path,text.data(),text.size());
    }
    static void error_file(const std::filesystem::path& directory,std::string_view text) noexcept {
        // Even composing a filesystem path can allocate. An allocation failure
        // while reporting an earlier capture failure must not terminate audio.
        try {text_file(directory/"export-error.txt",text);}catch(...) {}
    }
    static WriteResult wav_file(const std::filesystem::path& path,const AudioCaptureBuffer::Stream& stream) {
        const uint32_t pcm_bytes=stream.frames*4;std::array<char,44> wav{};
        auto u16=[&](size_t at,uint16_t value){wav[at]=char(value);wav[at+1]=char(value>>8);};
        auto u32=[&](size_t at,uint32_t value){for(unsigned b=0;b<4;b++)wav[at+b]=char(value>>(b*8));};
        std::memcpy(wav.data(),"RIFF",4);u32(4,pcm_bytes+36);std::memcpy(wav.data()+8,"WAVEfmt ",8);
        u32(16,16);u16(20,1);u16(22,2);u32(24,48000);u32(28,192000);u16(32,4);u16(34,16);
        std::memcpy(wav.data()+36,"data",4);u32(40,pcm_bytes);
        return exclusive_parts(path,{{wav.data(),wav.size()},{stream.pcm.get(),pcm_bytes}});
    }
    static std::string trace(const AudioCaptureBuffer::Stream& stream) {
        std::ostringstream text;text.imbue(std::locale::classic());text.precision(17);
        text<<"wall_s\toffset_frames\tepoch\trequested\tstored\taccepted\tdelivered\tmissing\twarmup\tpaused\tflags\tsame_epoch_interval_s\n";
        for(uint32_t i=0;i<stream.event_count;i++) {
            const auto& e=stream.events[i];
            text<<e.wall_s<<'\t'<<e.offset_frames<<'\t'<<e.epoch<<'\t'<<e.requested<<'\t'<<e.stored<<'\t'<<e.accepted<<'\t'<<e.delivered<<'\t'<<e.missing<<'\t'<<e.warmup<<'\t'<<e.paused<<'\t'<<e.flags<<'\t';
            if(e.flags&CaptureHasInterval)text<<e.interval_s;else text<<"nan";
            text<<'\n';
        }
        return text.str();
    }
    bool export_files(double elapsed,bool window_complete) noexcept {
        if(!buffer_.quiescent())return false;
        try {
            const auto& a=buffer_.source_;const auto& b=buffer_.returned_;
            const auto source=wav_file(directory_/"source.wav",a);
            const auto returned=wav_file(directory_/"returned.wav",b);
            const auto pushes=text_file(directory_/"push.tsv",trace(a));
            const auto reads=text_file(directory_/"read.tsv",trace(b));
            const bool files_ok=source.ok && returned.ok && pushes.ok && reads.ok;
            const bool lost=a.capped_frames || b.capped_frames || a.dropped_events || b.dropped_events;
            std::ostringstream manifest;manifest.imbue(std::locale::classic());manifest.precision(17);manifest<<std::boolalpha;
            manifest<<"{\n  \"schema\":1,\n  \"sample_rate\":48000,\n  \"channels\":2,\n  \"sample_format\":\"S16LE\",\n"
                <<"  \"complete\":"<<(files_ok && window_complete && !lost)<<",\n  \"window_complete\":"<<window_complete
                <<",\n  \"partial\":"<<(!window_complete)<<",\n  \"files_complete\":"<<files_ok<<",\n  \"capture_loss\":"<<lost
                <<",\n  \"elapsed_wall_seconds\":"<<elapsed<<",\n  \"target_wall_seconds\":"<<seconds_
                <<",\n  \"frame_capacity_per_stream\":"<<buffer_.frame_capacity_<<",\n  \"event_capacity_per_stream\":"<<buffer_.event_capacity_
                <<",\n  \"source_frames\":"<<a.frames<<",\n  \"returned_frames\":"<<b.frames
                <<",\n  \"source_requested_frames\":"<<a.requested<<",\n  \"returned_requested_frames\":"<<b.requested
                <<",\n  \"source_capped_frames\":"<<a.capped_frames<<",\n  \"returned_capped_frames\":"<<b.capped_frames
                <<",\n  \"push_events\":"<<a.event_count<<",\n  \"read_events\":"<<b.event_count
                <<",\n  \"dropped_push_events\":"<<a.dropped_events<<",\n  \"dropped_read_events\":"<<b.dropped_events
                <<",\n  \"max_same_epoch_unpaused_callback_gap_ms\":"<<b.max_unpaused_gap*1000
                <<",\n  \"max_same_epoch_running_callback_gap_ms\":"<<b.max_running_gap*1000
                <<",\n  \"same_epoch_unpaused_callback_intervals\":"<<b.unpaused_intervals
                <<",\n  \"same_epoch_running_callback_intervals\":"<<b.running_intervals
                <<",\n  \"source_wav_bytes_written\":"<<source.bytes<<",\n  \"returned_wav_bytes_written\":"<<returned.bytes
                <<",\n  \"push_trace_bytes_written\":"<<pushes.bytes<<",\n  \"read_trace_bytes_written\":"<<reads.bytes
                <<",\n  \"source_wav_ok\":"<<source.ok<<",\n  \"returned_wav_ok\":"<<returned.ok
                <<",\n  \"push_trace_ok\":"<<pushes.ok<<",\n  \"read_trace_ok\":"<<reads.ok<<"\n}\n";
            const auto result=text_file(directory_/"manifest.json",manifest.str());
            if(!result.ok)error_file(directory_,"Capture manifest could not be created; export is incomplete. No files were overwritten.\n");
            return result.ok && files_ok;
        } catch(...) {
            error_file(directory_,"Capture export failed; recording is incomplete. No files were overwritten.\n");return false;
        }
    }
    void work() noexcept {
        while(!stopping_.load(std::memory_order_acquire) && std::chrono::duration<double>(Clock::now()-started_).count()<seconds_)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const double elapsed=std::chrono::duration<double>(Clock::now()-started_).count();
        buffer_.freeze();
        // Only copies admitted before freeze can delay this check. It does not
        // require another callback and never delays the DSP worker or callback.
        while(!buffer_.quiescent())std::this_thread::yield();
        export_files(elapsed,elapsed>=seconds_);
    }
    AudioCapture(std::filesystem::path directory,uint32_t frames,uint32_t events,double seconds)
        :directory_(std::move(directory)),buffer_(frames,events),seconds_(seconds) {
        std::ostringstream started;started.imbue(std::locale::classic());started<<"{\"schema\":1,\"state\":\"armed\",\"target_wall_seconds\":"<<seconds_<<",\"sample_rate\":48000,\"channels\":2}\n";
        text_file(directory_/"started.json",started.str());writer_=std::thread([this]{work();});
    }
    static std::unique_ptr<AudioCapture> from_environment_impl(uint32_t frames,uint32_t events,double seconds) noexcept {
        std::filesystem::path directory;
        try {
            const char* value=std::getenv("GUNBLADE_AUDIO_LOG");if(!value || !value[0])return {};
            const std::filesystem::path log(value);if(!log.is_absolute() || !log.has_parent_path())return {};
            const auto parent=log.parent_path();const auto marker=parent/"audio-capture.on";
            std::error_code error;
            if(std::filesystem::symlink_status(marker,error).type()!=std::filesystem::file_type::regular)return {};
            static std::atomic<uint64_t> serial{0};
            for(unsigned attempt=0;attempt<64;attempt++) {
                const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
                const auto candidate=parent/("audio-capture-"+std::to_string(stamp)+"-"+std::to_string(serial.fetch_add(1)));
                if(std::filesystem::create_directory(candidate,error)){directory=candidate;break;}
                if(error && error!=std::errc::file_exists)return {};
            }
            if(directory.empty())return {};
            std::filesystem::rename(marker,directory/"request.on",error);
            if(error){std::filesystem::remove(directory,error);return {};}
            // Only an explicitly consumed new request authorizes this narrowly
            // scoped repair of earlier capture output. No-marker launches skip it.
            repair_prior_capture_permissions(parent);
            return std::unique_ptr<AudioCapture>(new AudioCapture(directory,frames,events,seconds));
        } catch(...) {
            if(!directory.empty())error_file(directory,"Capture initialization failed; audio continues without capture.\n");
            return {};
        }
    }
public:
    static std::unique_ptr<AudioCapture> from_environment() noexcept {
        return from_environment_impl(AudioCaptureBuffer::FrameCapacity,AudioCaptureBuffer::EventCapacity,60.);
    }
    ~AudioCapture() {stop();}
    void stop() noexcept {
        stopping_.store(true,std::memory_order_release);if(writer_.joinable())writer_.join();
    }
    void push(const int16_t* data,uint32_t frames,double now,uint64_t control,uint32_t accepted) noexcept {
        buffer_.push(data,frames,now,control,accepted);
    }
    void read(const int16_t* data,uint32_t frames,double now,uint64_t control,uint32_t delivered,uint32_t warmup,bool running) noexcept {
        buffer_.read(data,frames,now,control,delivered,warmup,running);
    }
#ifdef GUNBLADE_AUDIO_CAPTURE_TEST
    static bool capture_directory_name_for_test(std::string_view name) noexcept {return capture_directory_name(name);}
    static std::unique_ptr<AudioCapture> from_environment_for_test(uint32_t frames,uint32_t events,double seconds) noexcept {
        return from_environment_impl(frames,events,seconds);
    }
    static std::unique_ptr<AudioCapture> directory_for_test(const std::filesystem::path& directory,uint32_t frames,uint32_t events,double seconds) {
        return std::unique_ptr<AudioCapture>(new AudioCapture(directory,frames,events,seconds));
    }
    const std::filesystem::path& directory_for_test() const {return directory_;}
#endif
};
}
#endif
