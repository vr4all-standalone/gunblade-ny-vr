#include "../native/scene_diagnostics.h"
#include <cassert>
#include <sstream>
#include <chrono>
int main(int argc,char** argv){
    assert(argc==2);std::filesystem::path root=std::filesystem::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    gunblade::NativeFrame frame;frame.sequence=77;frame.width=frame.height=2;
    frame.native={0x12345678,0,0xff112233,0xffffffff};frame.background=frame.foreground=frame.native;
    frame.blobs.push_back({"palette",{1,2,3,4}});
    gunblade::NativePolygon p;p.count=3;p.object=0x80cb22;p.vertices[0]={1,2,3,4,5};frame.polygons.push_back(p);
    auto destination=gunblade::diagnostics::write_frame(frame,root);
    std::ifstream meta(destination/"metadata.json");std::string text{std::istreambuf_iterator<char>(meta),{}};
    assert(text.find("\"sequence\":77")!=std::string::npos&&text.find("\"object\":8440610")!=std::string::npos);
    std::ifstream pixels(destination/"native-argb32.bin",std::ios::binary);std::vector<unsigned char> data{std::istreambuf_iterator<char>(pixels),{}};
    assert(data.size()==16&&data[0]==0x78&&data[1]==0x56&&data[2]==0x34&&data[3]==0x12);
    assert(!std::filesystem::exists(destination/"metadata.partial"));
    bool refused=false;try{gunblade::diagnostics::write_frame(frame,root);}catch(const std::exception&){refused=true;}
    assert(refused&&std::filesystem::file_size(destination/"palette.bin")==4);
    const auto sessions=root/"sessions";_putenv_s("GUNBLADE_RETICLE_DIAGNOSTICS",sessions.string().c_str());
    auto snapshot=std::make_shared<const gunblade::NativeFrame>([&]{auto f=frame;f.time=90;return f;}());
    gunblade::NativeReticleDiagnostics status;gunblade::GameState state;
    for(int launch=0;launch<2;launch++){
        gunblade::SceneDiagnostics recording;
        recording.observe(status,state,snapshot,[](const std::string&){});
    }
    _putenv_s("GUNBLADE_RETICLE_DIAGNOSTICS","");
    unsigned complete=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(sessions))if(entry.path().filename()=="metadata.json")++complete;
    assert(complete==2); // Relaunches restart frame numbers; both sessions survive.
}
