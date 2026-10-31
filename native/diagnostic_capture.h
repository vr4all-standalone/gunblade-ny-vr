// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
namespace gunblade::diagnostics {
template<class Range> inline void array(std::ostream& out,const Range& values){out<<'[';bool first=true;for(auto v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
inline std::filesystem::path write_frame(const NativeFrame& frame,const std::filesystem::path& root){
    std::ostringstream name;name<<"frame-"<<std::setw(8)<<std::setfill('0')<<frame.sequence;
    std::filesystem::create_directories(root);auto folder=root/name.str();
    if(!std::filesystem::create_directory(folder))throw std::runtime_error("Diagnostic frame already exists");
    auto file=[&](const std::string& name){std::ofstream out(folder/name,std::ios::binary);out.exceptions(std::ios::failbit|std::ios::badbit);return out;};
    auto out=file("metadata.partial");out.imbue(std::locale::classic());out<<std::setprecision(9);
    out<<"{\"schema\":1,\"sequence\":"<<frame.sequence<<",\"time\":"<<frame.time<<",\"width\":"<<frame.width<<",\"height\":"<<frame.height
       <<",\"xoffset\":"<<frame.xoffset<<",\"yoffset\":"<<frame.yoffset<<",\"rejected\":"<<frame.rejected<<",\"blobs\":[";
    bool first=true;
    for(const auto& blob:frame.blobs){
        if(blob.name.empty()||blob.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)throw std::runtime_error("Unsafe diagnostic blob name");
        auto bytes=file(blob.name+".bin");bytes.write(reinterpret_cast<const char*>(blob.bytes.data()),blob.bytes.size());bytes.close();
        if(!first)out<<',';
        first=false;out<<"{\"name\":\""<<blob.name<<"\",\"size\":"<<blob.bytes.size()<<'}';
    }
    out<<"],\"polygons\":[";first=true;
    for(const auto& p:frame.polygons){
        if(p.count>4)throw std::runtime_error("Invalid diagnostic vertex count");
        if(!first)out<<',';
        first=false;
        out<<"{\"attr\":"<<p.attr<<",\"object\":"<<p.object<<",\"window\":"<<p.window<<",\"sort\":"<<p.sort<<",\"luma\":"<<p.luma<<",\"lod\":"<<p.lod<<",\"culled\":"<<(p.culled?"true":"false")<<",\"header\":";
        array(out,p.header);out<<",\"focus\":";array(out,p.focus);out<<",\"center\":";array(out,p.center);out<<",\"viewport\":";array(out,p.viewport);out<<",\"matrix\":";array(out,p.matrix);out<<",\"vertices\":[";
        for(unsigned i=0;i<p.count;i++){if(i)out<<',';const auto& v=p.vertices[i];out<<'['<<v.x<<','<<v.y<<','<<v.z<<','<<v.u<<','<<v.v<<']';}
        out<<"]}";
    }
    out<<"]}\n";out.close();
    auto raster=[&](const char* name,const std::vector<uint32_t>& pixels){auto bytes=file(name);for(auto p:pixels)for(int c=0;c<4;c++)bytes.put(char(p>>(c*8)));bytes.close();};
    raster("background-argb32.bin",frame.background);raster("foreground-argb32.bin",frame.foreground);raster("native-argb32.bin",frame.native);
    std::filesystem::rename(folder/"metadata.partial",folder/"metadata.json");return folder;
}
}
