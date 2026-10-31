// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
#include <bit>
#include <map>
namespace gunblade {
struct NativeReticleSource {
    std::vector<size_t> polygons;
    std::vector<size_t> duplicates;
    double center_x{},center_y{};
    unsigned window{};
    bool valid()const{return !polygons.empty()&&std::isfinite(center_x)&&std::isfinite(center_y);}
};
using NativeReticles=std::array<NativeReticleSource,2>;
enum NativeReticleReason : uint32_t {
    ReticleFrameIncomplete=1u<<0,ReticleFrameRejected=1u<<1,ReticleDisplayMismatch=1u<<2,
    ReticleResourcesMissing=1u<<3,ReticleGeometryMissing=1u<<4,ReticleNoKnownSource=1u<<5,
    ReticleTopWindowMissing=1u<<6,ReticlePartCountMismatch=1u<<7,ReticleNativeLinkMismatch=1u<<8,
    ReticleMaterialMismatch=1u<<9,ReticleFocusMismatch=1u<<10,ReticleCenterMismatch=1u<<11,
    ReticleTransformMismatch=1u<<12,ReticleOriginMismatch=1u<<13,ReticleVertexMismatch=1u<<14,
    ReticleDuplicateCountMismatch=1u<<15,ReticleDuplicateFieldsMismatch=1u<<16
};
struct NativeReticlePlayerDiagnostics {
    uint32_t reason_flags{};
    unsigned known_polygons{},selected_polygons{},suppressed_polygons{},visible_polygons{};
    std::array<unsigned,8> windows{};
    unsigned other_windows{};
    std::array<unsigned,2> top_parts{},lower_parts{};
    uint32_t first_object{};
    unsigned selected_window{};
    std::array<float,2> first_focus{};
    std::array<float,3> first_origin{};
};
struct NativeReticleDiagnostics {
    uint64_t sequence{};
    size_t polygon_count{};
    unsigned frame_rejected{},unknown_focus600_polygons{};
    bool frame_complete{},resources_valid{},geometry_available{},scene_usable{};
    std::array<NativeReticlePlayerDiagnostics,2> players;
};
// Revision A native lists, independently observed in solo/dual captures at
// 20.026368, 35.046144 and 40.052736 seconds. Each ring is 19 quads; its moving
// chevrons add nine quads and two triangles. The source list and solid material
// identity are both required: window, focus, color or depth alone are not IDs.
inline NativeReticles native_reticles(const NativeFrame& frame,NativeReticleDiagnostics* diagnostics=nullptr) {
    NativeReticleDiagnostics observed;
    observed.sequence=frame.sequence;observed.polygon_count=frame.polygons.size();
    observed.frame_rejected=frame.rejected;observed.frame_complete=frame.complete;
    NativeReticles result;
    uint32_t frame_errors=(!frame.complete?ReticleFrameIncomplete:0u)|(frame.rejected?ReticleFrameRejected:0u)
        |(frame.width!=496||frame.height!=384?ReticleDisplayMismatch:0u);
    if(frame_errors){for(auto& player:observed.players)player.reason_flags=frame_errors;if(diagnostics)*diagnostics=observed;return result;}
    constexpr uint32_t objects[2][2]={{0x80cb22,0x80ccac},{0x80cbe7,0x80cde6}};
    constexpr unsigned colors[2][2]={{384,10496},{11584,11648}};
    struct Group {NativeReticleSource source;std::array<unsigned,2> parts{};std::array<float,2> translation{};bool invalid{};};
    std::array<std::map<unsigned,Group>,2> groups;
    for(size_t index=0;index<frame.polygons.size();++index) {
        const auto& p=frame.polygons[index];int player=-1,part=-1;
        for(int who=0;who<2;who++)for(int what=0;what<2;what++)if(p.object==objects[who][what]){player=who;part=what;}
        if(player<0){if(p.focus==std::array<float,2>{600,600})++observed.unknown_focus600_polygons;continue;}
        auto& stats=observed.players[player];
        if(stats.known_polygons++==0){stats.first_object=p.object;stats.first_focus=p.focus;stats.first_origin={p.matrix[9],p.matrix[10],p.matrix[11]};}
        if(p.window<stats.windows.size())++stats.windows[p.window];else ++stats.other_windows;
        auto& group=groups[player][p.window];
        auto& source=group.source;auto& bad=group.invalid;const auto& m=p.matrix;
        if(source.polygons.empty()) {
            group.translation={m[9],m[10]};source.window=p.window;
            source.center_x=frame.xoffset+p.center[0]+600.*m[9]/10.;
            source.center_y=384-p.center[1]+frame.yoffset-600.*m[10]/10.;
        }
        const bool native_link=p.count==4?(p.attr==132609||p.attr==132097):(p.attr==132354||p.attr==132610);
        auto reject=[&](NativeReticleReason reason,bool condition){if(condition){bad=true;stats.reason_flags|=reason;}};
        reject(ReticleNativeLinkMismatch,!native_link||p.culled!=(p.attr==132097));
        reject(ReticleMaterialMismatch,p.luma!=255||p.header!=std::array<unsigned,4>{192,0,0,colors[player][part]});
        reject(ReticleFocusMismatch,p.focus!=std::array<float,2>{600,600});
        reject(ReticleCenterMismatch,p.center!=std::array<float,2>{248,320});
        reject(ReticlePartCountMismatch,p.count<3||p.count>4||(part==0&&p.count!=4));
        // The original planar scale is .08, with animated in-plane rotation.
        // Reject another use of these source lists or inconsistent gun origins.
        for(float value:m)reject(ReticleTransformMismatch,!std::isfinite(value));
        reject(ReticleTransformMismatch,std::abs(m[0]*m[0]+m[1]*m[1]-.0064f)>1e-6f
            ||std::abs(m[3]*m[3]+m[4]*m[4]-.0064f)>1e-6f
            ||std::abs(m[0]*m[3]+m[1]*m[4])>1e-6f
            ||std::abs(m[2])+std::abs(m[5])+std::abs(m[6])+std::abs(m[7])>1e-6f
            ||std::abs(m[8]-.08f)>1e-6f||std::abs(m[11]-10)>1e-6f);
        reject(ReticleOriginMismatch,std::abs(m[9]-group.translation[0])>1e-6f||std::abs(m[10]-group.translation[1])>1e-6f);
        for(unsigned vertex=0;vertex<p.count&&vertex<4;vertex++) {
            const auto& v=p.vertices[vertex];
            reject(ReticleVertexMismatch,!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)
                ||!std::isfinite(v.u)||!std::isfinite(v.v)||std::abs(v.z-10)>1e-6f);
        }
        ++group.parts[part];source.polygons.push_back(index);
    }
    auto same=[](const NativePolygon& a,const NativePolygon& b){
        if(a.count<3||a.count>4||b.count<3||b.count>4||a.count!=b.count||a.attr!=b.attr||a.sort!=b.sort||a.luma!=b.luma||a.lod!=b.lod
            ||a.header!=b.header||a.focus!=b.focus||a.center!=b.center||a.viewport!=b.viewport
            ||a.object!=b.object||a.culled!=b.culled)return false;
        for(size_t i=0;i<a.matrix.size();i++) {
            const float x=a.matrix[i],y=b.matrix[i];if(x==y)continue;
            if(i!=0&&i!=1&&i!=3&&i!=4)return false; // Origin and other axes stay exact.
            if(!std::isfinite(x)||!std::isfinite(y)||std::signbit(x)!=std::signbit(y))return false;
            const auto bx=std::bit_cast<uint32_t>(x),by=std::bit_cast<uint32_t>(y);
            if((bx>by?bx-by:by-bx)>2)return false;
        }
        for(unsigned i=0;i<a.count;i++) {
            const auto& x=a.vertices[i];const auto& y=b.vertices[i];
            if(x.z!=y.z||x.z<=0||x.u!=y.u||x.v!=y.v||!std::isfinite(x.x)||!std::isfinite(x.y)
                ||!std::isfinite(y.x)||!std::isfinite(y.y))return false;
            // Actual repeated native submissions differ by 1-2 rotation ULPs.
            // The raster command then drops eight float mantissa bits, so even
            // identical artwork need not have byte-identical transformed X/Y.
            // Compare native projected positions to a hundredth source pixel;
            // preserve exact depth, UV, origin, source and material identity.
            if(std::abs(double(x.x)/x.z-double(y.x)/y.z)>.01
                ||std::abs(double(x.y)/x.z-double(y.y)/y.z)>.01)return false;
        }
        return true;
    };
    for(unsigned player=0;player<2;player++) {
        auto& source=result[player];auto& stats=observed.players[player];
        if(groups[player].empty()){stats.reason_flags|=ReticleNoKnownSource|ReticlePartCountMismatch;continue;}
        // Native windows count Window Data commands, not semantic HUD layers.
        // Actual Quest area2 submits the same sources in window2, then2/3,
        // then1/2. Use the highest audited complete source only when every
        // other submission of that player's source is a verified duplicate.
        const auto& top=groups[player].rbegin()->second;
        stats.top_parts=top.parts;bool invalid=false;
        for(const auto& [window,group]:groups[player]){
            invalid=invalid||group.invalid;
            if(group.parts!=std::array<unsigned,2>{19,11}){invalid=true;stats.reason_flags|=ReticlePartCountMismatch;}
            if(window==top.source.window)continue;
            for(unsigned part=0;part<2;part++)stats.lower_parts[part]+=group.parts[part];
            if(group.source.polygons.size()!=top.source.polygons.size()){invalid=true;stats.reason_flags|=ReticleDuplicateCountMismatch;}
            else for(size_t i=0;i<group.source.polygons.size();i++)
                if(!same(frame.polygons[top.source.polygons[i]],frame.polygons[group.source.polygons[i]])){invalid=true;stats.reason_flags|=ReticleDuplicateFieldsMismatch;}
        }
        if(!invalid){
            source=top.source;stats.selected_window=source.window;
            for(const auto& [window,group]:groups[player])source.duplicates.insert(source.duplicates.end(),group.source.polygons.begin(),group.source.polygons.end());
            stats.selected_polygons=unsigned(source.polygons.size());stats.suppressed_polygons=unsigned(source.duplicates.size());
            for(size_t index:source.polygons)if(!frame.polygons[index].culled)++stats.visible_polygons;
        }
    }
    if(diagnostics)*diagnostics=observed;
    return result;
}
}
