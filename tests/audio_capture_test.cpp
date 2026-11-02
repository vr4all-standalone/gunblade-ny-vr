// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/audio_stretch.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <new>

static thread_local bool capture_rt_checked=false;
static thread_local uint64_t capture_rt_allocations=0;
static std::atomic<bool> capture_fail_new{false};
[[gnu::noinline]] void* operator new(size_t bytes) {
    if(capture_rt_checked)++capture_rt_allocations;
    if(capture_fail_new.load(std::memory_order_relaxed))throw std::bad_alloc();
    if(void* data=std::malloc(bytes?bytes:1))return data;throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void* data) noexcept {std::free(data);}
[[gnu::noinline]] void operator delete(void* data,size_t) noexcept {std::free(data);}
void* operator new[](size_t bytes) {return ::operator new(bytes);}
void operator delete[](void* data) noexcept {::operator delete(data);}
void operator delete[](void* data,size_t) noexcept {::operator delete(data);}
[[gnu::noinline]] void* operator new(size_t bytes,std::align_val_t alignment) {
    if(capture_rt_checked)++capture_rt_allocations;
    if(capture_fail_new.load(std::memory_order_relaxed))throw std::bad_alloc();
    const size_t align=size_t(alignment),rounded=(bytes+align-1)/align*align;
#ifdef _WIN32
    if(void* data=_aligned_malloc(rounded,align))return data;
#else
    if(void* data=std::aligned_alloc(align,rounded))return data;
#endif
    throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void* data,std::align_val_t) noexcept {
#ifdef _WIN32
    _aligned_free(data);
#else
    std::free(data);
#endif
}
[[gnu::noinline]] void operator delete(void* data,size_t,std::align_val_t alignment) noexcept {::operator delete(data,alignment);}
void* operator new[](size_t bytes,std::align_val_t alignment) {return ::operator new(bytes,alignment);}
void operator delete[](void* data,std::align_val_t alignment) noexcept {::operator delete(data,alignment);}
void operator delete[](void* data,size_t,std::align_val_t alignment) noexcept {::operator delete(data,alignment);}

namespace fs=std::filesystem;
static void environment(const char* name,const std::string& value) {
#ifdef _WIN32
    _putenv_s(name,value.c_str());
#else
    if(value.empty())unsetenv(name);else setenv(name,value.c_str(),1);
#endif
}
static std::string read_text(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);assert(input);return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
static uint64_t number(const std::string& json,const std::string& key) {
    const size_t begin=json.find('"'+key+'"');assert(begin!=std::string::npos);return std::stoull(json.substr(json.find(':',begin)+1));
}
static double real_number(const std::string& json,const std::string& key) {
    const size_t begin=json.find('"'+key+'"');assert(begin!=std::string::npos);return std::stod(json.substr(json.find(':',begin)+1));
}
static std::vector<int16_t> wav_samples(const fs::path& path) {
    const auto bytes=read_text(path);assert(bytes.size()>=44 && bytes.substr(0,4)=="RIFF" && bytes.substr(36,4)=="data");
    auto u32=[&](size_t at){uint32_t result=0;for(unsigned b=0;b<4;b++)result|=uint32_t(uint8_t(bytes[at+b]))<<(b*8);return result;};
    assert(u32(24)==48000 && u32(40)+44==bytes.size());
    std::vector<int16_t> result((bytes.size()-44)/2);std::memcpy(result.data(),bytes.data()+44,bytes.size()-44);return result;
}
static std::vector<std::array<std::string,12>> trace_rows(const fs::path& path) {
    std::istringstream input(read_text(path));std::string line;std::getline(input,line);
    std::vector<std::array<std::string,12>> result;
    while(std::getline(input,line)) {std::istringstream row(line);std::array<std::string,12> fields;for(auto& field:fields)assert(bool(std::getline(row,field,'\t')));result.push_back(std::move(fields));}
    return result;
}
static fs::path child(const fs::path& root,const char* name) {const auto result=root/name;assert(fs::create_directory(result));return result;}

#if defined(GUNBLADE_AUDIO_DIAGNOSTICS) && GUNBLADE_AUDIO_DIAGNOSTICS
using namespace gunblade_audio_detail;
static void gate_freeze_without_future_callback() {
    CaptureGate gate;assert(gate.enter());gate.close();assert(!gate.quiescent());assert(!gate.enter());
    assert(!gate.late_entry_for_test());assert(!gate.quiescent());
    gate.leave();assert(gate.quiescent());for(unsigned i=0;i<100;i++)assert(!gate.enter());
    assert(!gate.late_entry_for_test());assert(gate.quiescent());
}
static void exact_copy_caps_and_partial_export(const fs::path& root) {
    const auto directory=child(root,"caps");auto capture=AudioCapture::directory_for_test(directory,17,2,10.);
    std::array<int16_t,32> input;for(size_t i=0;i<input.size();i++)input[i]=int16_t(i*123-1000);
    const auto expected=input;capture_rt_allocations=0;capture_rt_checked=true;
    for(unsigned i=0;i<3;i++){capture->push(input.data(),16,100.+i*.02,0,16);capture->read(input.data(),8,100.+i*.02,0,i?8:0,i?0:8,i!=0);}
    capture_rt_checked=false;assert(capture_rt_allocations==0 && input==expected);
    input.fill(0);const auto stopped=std::chrono::steady_clock::now();capture->stop();
    assert(std::chrono::duration<double>(std::chrono::steady_clock::now()-stopped).count()<.5);
    const auto a=wav_samples(directory/"source.wav"),b=wav_samples(directory/"returned.wav");
    assert(a.size()==34 && b.size()==34);for(size_t i=0;i<a.size();i++)assert(a[i]==expected[i%32]);
    for(size_t i=0;i<b.size();i++)assert(b[i]==expected[i%16]);
    const auto manifest=read_text(directory/"manifest.json");assert(manifest.find("\"partial\":true")!=std::string::npos);
    assert(number(manifest,"source_frames")==17 && number(manifest,"returned_frames")==17);
    assert(number(manifest,"source_capped_frames")==31 && number(manifest,"returned_capped_frames")==7);
    assert(number(manifest,"dropped_push_events")==1 && number(manifest,"dropped_read_events")==1);
    assert(manifest.find("\"complete\":false")!=std::string::npos && manifest.find("\"files_complete\":true")!=std::string::npos);
    assert(fs::file_size(directory/"source.wav")==number(manifest,"source_wav_bytes_written"));
    assert(fs::file_size(directory/"read.tsv")==number(manifest,"read_trace_bytes_written"));
    const auto rows=trace_rows(directory/"read.tsv");assert(rows.size()==2 && rows[0][8]=="8" && rows[1][6]=="8");
}
static void exact_epoch_gaps_and_counts(const fs::path& root) {
    const auto directory=child(root,"gaps");auto capture=AudioCapture::directory_for_test(directory,128,16,10.);std::array<int16_t,16> data{};
    capture->read(data.data(),8,100.,0,8,0,true);
    capture->read(data.data(),8,100.02,0,8,0,true);
    capture->read(data.data(),8,200.,3,0,0,false);
    capture->read(data.data(),8,300.,4,0,8,false);
    capture->read(data.data(),8,300.01,4,8,0,true);
    capture->read(data.data(),8,300.04,4,6,0,true);
    capture->read(data.data(),8,std::numeric_limits<double>::quiet_NaN(),4,8,0,true);
    capture->read(data.data(),8,301.,4,8,0,true);capture->stop();
    const auto rows=trace_rows(directory/"read.tsv");assert(rows.size()==8);
    assert(rows[0][11]=="nan" && std::abs(std::stod(rows[1][11])-.02)<1e-10);
    assert(rows[2][2]=="1" && rows[2][9]=="8" && rows[2][11]=="nan");
    assert(rows[3][2]=="2" && rows[3][8]=="8" && rows[3][11]=="nan");
    assert(rows[5][6]=="6" && rows[5][7]=="2" && rows[5][8]=="0");
    assert(rows[6][11]=="nan" && rows[7][11]=="nan");
    const auto manifest=read_text(directory/"manifest.json");
    assert(std::abs(real_number(manifest,"max_same_epoch_unpaused_callback_gap_ms")-30.)<1e-7);
    assert(std::abs(real_number(manifest,"max_same_epoch_running_callback_gap_ms")-30.)<1e-7);
    assert(number(manifest,"same_epoch_unpaused_callback_intervals")==3 && number(manifest,"same_epoch_running_callback_intervals")==2);
}
static void concurrent_deadline_freeze(const fs::path& root) {
    const auto directory=child(root,"concurrent");auto capture=AudioCapture::directory_for_test(directory,512,64,.03);
    std::atomic<bool> done{false};std::atomic<uint64_t> iterations{0};
    auto writer=[&](bool source) {
        std::array<int16_t,64> data;data.fill(source?1234:-2345);uint64_t at=0;capture_rt_allocations=0;capture_rt_checked=true;
        while(!done.load(std::memory_order_relaxed)) {
            if(source)capture->push(data.data(),32,100.+at*.001,0,32);else capture->read(data.data(),32,100.+at*.001,0,32,0,true);
            ++at;iterations.fetch_add(1,std::memory_order_relaxed);
        }
        capture_rt_checked=false;assert(capture_rt_allocations==0);
    };
    std::thread producer(writer,true),consumer(writer,false);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));capture->stop();done.store(true);producer.join();consumer.join();
    assert(iterations.load()>128);
    const auto before=read_text(directory/"returned.wav");std::array<int16_t,64> data{};capture->read(data.data(),32,400.,0,32,0,true);capture->stop();
    assert(read_text(directory/"returned.wav")==before);
    const auto a=wav_samples(directory/"source.wav"),b=wav_samples(directory/"returned.wav");
    assert(a.size()==1024 && b.size()==1024);for(auto value:a)assert(value==1234);for(auto value:b)assert(value==-2345);
    const auto manifest=read_text(directory/"manifest.json");assert(manifest.find("\"window_complete\":true")!=std::string::npos);
    assert(manifest.find("\"files_complete\":true")!=std::string::npos && manifest.find("\"capture_loss\":true")!=std::string::npos);
    assert(number(manifest,"dropped_read_events")>0 && number(manifest,"dropped_push_events")>0);
}
static void export_never_overwrites(const fs::path& root) {
    const auto directory=child(root,"no-overwrite");std::ofstream(directory/"source.wav")<<"KEEP";
    auto capture=AudioCapture::directory_for_test(directory,8,4,10.);std::array<int16_t,16> data{};capture->push(data.data(),8,100.,0,8);capture->stop();
    assert(read_text(directory/"source.wav")=="KEEP");const auto manifest=read_text(directory/"manifest.json");
    assert(manifest.find("\"source_wav_ok\":false")!=std::string::npos && manifest.find("\"files_complete\":false")!=std::string::npos);
    const auto second=child(root,"manifest-collision");std::ofstream(second/"manifest.json")<<"KEEP";
    capture=AudioCapture::directory_for_test(second,8,4,10.);capture->stop();
    assert(read_text(second/"manifest.json")=="KEEP" && fs::is_regular_file(second/"export-error.txt"));
}
static void completed_window_and_marker_validation(const fs::path& root) {
    const auto directory=child(root,"complete");auto capture=AudioCapture::directory_for_test(directory,128,16,.01);
    std::array<int16_t,16> data{};capture->push(data.data(),8,100.,0,8);capture->read(data.data(),8,100.,0,8,0,true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));capture->stop();
    const auto manifest=read_text(directory/"manifest.json");assert(manifest.find("\"complete\":true")!=std::string::npos);
    assert(manifest.find("\"partial\":false")!=std::string::npos && manifest.find("\"capture_loss\":false")!=std::string::npos);
    assert(real_number(manifest,"elapsed_wall_seconds")>=.01);
    const auto invalid=child(root,"invalid-marker");assert(fs::create_directory(invalid/"audio-capture.on"));
    environment("GUNBLADE_AUDIO_LOG",(invalid/"gunblade-audio.tsv").string());assert(!AudioCapture::from_environment_for_test(8,4,.01));
    unsigned entries=0;for(const auto& entry:fs::directory_iterator(invalid)){++entries;assert(entry.path().filename()=="audio-capture.on");}assert(entries==1);
    fs::remove(invalid/"audio-capture.on");std::ofstream(invalid/"audio-capture.on").put('\n');
    environment("GUNBLADE_AUDIO_LOG","");assert(!AudioCapture::from_environment_for_test(8,4,.01));assert(fs::exists(invalid/"audio-capture.on"));
    environment("GUNBLADE_AUDIO_LOG","relative-audio.tsv");assert(!AudioCapture::from_environment_for_test(8,4,.01));assert(fs::exists(invalid/"audio-capture.on"));
}
static void allocation_failure_during_export_is_contained(const fs::path& root) {
    const auto directory=child(root,"allocation-failure");auto capture=AudioCapture::directory_for_test(directory,8,4,10.);
    // Both export serialization/path work and its error-report path must remain
    // contained if allocations fail. No manifest may claim a complete result.
    capture_fail_new.store(true,std::memory_order_release);capture->stop();capture_fail_new.store(false,std::memory_order_release);
    assert(!fs::exists(directory/"manifest.json"));
}
static void permission_repair_scope(const fs::path& root) {
    for(const char* name:{"audio-capture-1-0","audio-capture-1793640303593328-0"})assert(AudioCapture::capture_directory_name_for_test(name));
    for(const char* name:{"audio-capture-","audio-capture-a-1","audio-capture-1-","audio-capture--1","audio-capture-1-2-extra","audio-capture-1-2/child","settings.ini"})assert(!AudioCapture::capture_directory_name_for_test(name));
#ifndef _WIN32
    const auto parent=child(root,"permission-scope"),valid=child(parent,"audio-capture-123-4"),invalid=child(parent,"audio-capture-123-4-extra"),outside=child(root,"outside-permission-scope");
    auto create=[](const fs::path& path){std::ofstream(path)<<"KEEP";assert(::chmod(path.c_str(),0600)==0);};
    auto mode=[](const fs::path& path){struct stat state{};assert(::stat(path.c_str(),&state)==0);return state.st_mode&0777;};
    for(const char* name:{"request.on","started.json","source.wav","returned.wav","push.tsv","read.tsv","manifest.json","export-error.txt"})create(valid/name);
    create(valid/"settings.ini");const auto nested=child(valid,"nested");create(nested/"source.wav");create(invalid/"source.wav");create(parent/"private.wav");create(outside/"source.wav");
    fs::create_directory_symlink(outside,parent/"audio-capture-999-0");
    const auto links=child(parent,"audio-capture-222-0");fs::create_symlink(outside/"source.wav",links/"source.wav");
    std::error_code hardlink_error;
    fs::create_hard_link(outside/"source.wav",links/"returned.wav",hardlink_error);
    if(hardlink_error) {
        const bool unsupported=hardlink_error==std::errc::permission_denied || hardlink_error==std::errc::operation_not_permitted ||
            hardlink_error==std::errc::operation_not_supported || hardlink_error==std::errc::function_not_supported;
        assert(unsupported);
        std::fprintf(stderr,"SKIP hard-link permission fixture: platform rejected creation (%s); production nlink guard remains enabled\n",hardlink_error.message().c_str());
    }
    environment("GUNBLADE_AUDIO_LOG",(parent/"gunblade-audio.tsv").string());
    assert(!AudioCapture::from_environment_for_test(8,4,10.));assert(mode(valid/"source.wav")==0600); // No marker, no repair.
    create(parent/"audio-capture.on");const mode_t previous_umask=::umask(0077);
    auto capture=AudioCapture::from_environment_for_test(8,4,10.);assert(capture);const auto fresh=capture->directory_for_test();capture->stop();::umask(previous_umask);
    assert(!fs::exists(parent/"audio-capture.on"));
    for(const char* name:{"request.on","started.json","source.wav","returned.wav","push.tsv","read.tsv","manifest.json","export-error.txt"})assert(mode(valid/name)==0644 && read_text(valid/name)=="KEEP");
    for(const char* name:{"started.json","source.wav","returned.wav","push.tsv","read.tsv","manifest.json"})assert(mode(fresh/name)==0644);
    assert(mode(valid/"settings.ini")==0600 && mode(nested/"source.wav")==0600 && mode(invalid/"source.wav")==0600);
    assert(mode(parent/"private.wav")==0600 && mode(outside/"source.wav")==0600); // No symlink/hardlink escape.
    assert(::chmod((valid/"source.wav").c_str(),0600)==0);assert(!AudioCapture::from_environment_for_test(8,4,10.));assert(mode(valid/"source.wav")==0600); // One request only.
#else
    (void)root;
#endif
}
#endif
int main(int argc,char** argv) {
    assert(argc==2);
    const fs::path base=fs::absolute(argv[1]);fs::create_directories(base);
    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root=base/("audio-capture-test-"+std::to_string(unique));
    assert(fs::create_directory(root));
    const char* old_value=std::getenv("GUNBLADE_AUDIO_LOG");const std::string old_log=old_value?old_value:"";
    environment("GUNBLADE_AUDIO_LOG",(root/"gunblade-audio.tsv").string());
    void* worker=gunblade_audio_create(48000,2);assert(worker);gunblade_audio_destroy(worker);
    assert(fs::is_empty(root)); // Flag alone must never create capture output.
    std::ofstream(root/"audio-capture.on").put('\n');
    worker=gunblade_audio_create(48000,2);assert(worker);
    std::array<int16_t,1920> input;for(size_t i=0;i<input.size();i++)input[i]=int16_t(i*7-6000);
    std::array<int16_t,1024> output;std::vector<int16_t> expected_source,expected_returned;std::vector<uint32_t> delivered;
    auto read=[&](double at) {
        capture_rt_allocations=0;capture_rt_checked=true;const auto result=gunblade_audio_read(worker,output.data(),512,at);capture_rt_checked=false;assert(capture_rt_allocations==0);
        expected_returned.insert(expected_returned.end(),output.begin(),output.end());delivered.push_back(result);
    };
    read(100.);
    for(unsigned block=0;block<12;block++) {
        capture_rt_allocations=0;capture_rt_checked=true;assert(gunblade_audio_push(worker,input.data(),960,100.+block*.02)==960);capture_rt_checked=false;assert(capture_rt_allocations==0);
        expected_source.insert(expected_source.end(),input.begin(),input.end());std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));read(100.5);assert(delivered.back()==512);
    gunblade_audio_pause(worker,1);read(100.52);
    capture_rt_checked=true;assert(gunblade_audio_push(worker,input.data(),960,100.53)==0);capture_rt_checked=false;assert(capture_rt_allocations==0);
    expected_source.insert(expected_source.end(),input.begin(),input.end());gunblade_audio_pause(worker,0);read(100.54);
    gunblade_audio_destroy(worker);
#if defined(GUNBLADE_AUDIO_DIAGNOSTICS) && GUNBLADE_AUDIO_DIAGNOSTICS
    assert(!fs::exists(root/"audio-capture.on"));
    unsigned captures=0;
    for(const auto& entry:fs::directory_iterator(root))if(entry.is_directory()) {
        ++captures;assert(fs::is_regular_file(entry.path()/"manifest.json"));
        assert(wav_samples(entry.path()/"source.wav")==expected_source);
        assert(wav_samples(entry.path()/"returned.wav")==expected_returned);
        const auto rows=trace_rows(entry.path()/"read.tsv");assert(rows.size()==4);
        for(size_t i=0;i<rows.size();i++)assert(std::stoul(rows[i][6])==delivered[i]);
        assert(rows[0][8]=="512" && rows[1][6]=="512" && rows[2][9]=="512" && rows[3][8]=="512");
        const auto pushes=trace_rows(entry.path()/"push.tsv");assert(pushes.size()==13 && pushes.back()[5]=="0" && pushes.back()[9]=="960");
    }
    assert(captures==1);
    worker=gunblade_audio_create(48000,2);assert(worker);gunblade_audio_destroy(worker);
    unsigned after=0;for(const auto& entry:fs::directory_iterator(root))after+=entry.is_directory();assert(after==1); // Marker is one-use.
    gate_freeze_without_future_callback();exact_copy_caps_and_partial_export(root);exact_epoch_gaps_and_counts(root);
    concurrent_deadline_freeze(root);export_never_overwrites(root);completed_window_and_marker_validation(root);
    allocation_failure_during_export_is_contained(root);
    permission_repair_scope(root);
#else
    assert(fs::is_regular_file(root/"audio-capture.on"));
    unsigned directories=0;for(const auto& entry:fs::directory_iterator(root))directories+=entry.is_directory();
    assert(directories==0);
#endif
    environment("GUNBLADE_AUDIO_LOG",old_log);
    std::puts("Audio capture activation, exact PCM, timing, freeze, caps, RT allocation and safe export regressions passed");
}
