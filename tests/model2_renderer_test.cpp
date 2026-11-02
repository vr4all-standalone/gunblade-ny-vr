// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG // These executable regression checks must also run in Release.
#endif
#include "model2_scene.h"
#include "native_reticle.h"
#include "reticle_size.h"
#include "reticle_mode.h"
#include "custom_reticle.h"
#include <cassert>
#include <iostream>
#ifdef GUNBLADE_RENDERER_GL_TEST
#include "gl_scene.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
static PFNGLDRAWARRAYSPROC original_draw_arrays;
static unsigned observed_draw_calls,observed_world_draw_calls;
static void GLAD_API_PTR observe_draw_arrays(GLenum mode,GLint first,GLsizei count) {
    GLint current{};glGetIntegerv(GL_CURRENT_PROGRAM,&current);
    if(glGetUniformLocation(GLuint(current),"source_scanlines")>=0)++observed_world_draw_calls;
    ++observed_draw_calls;original_draw_arrays(mode,first,count);
}
static PFNGLTEXIMAGE2DPROC original_tex_image_2d;
static unsigned observed_resource_uploads,observed_raster_uploads;
static void GLAD_API_PTR observe_tex_image_2d(GLenum target,GLint level,GLint internal,GLsizei width,GLsizei height,
    GLint border,GLenum format,GLenum type,const void* pixels) {
    if(internal==GL_R8UI)++observed_resource_uploads;
    if(internal==GL_RGBA8)++observed_raster_uploads;
    original_tex_image_2d(target,level,internal,width,height,border,format,type,pixels);
}
#endif

using namespace gunblade;
static bool close(double a,double b){return std::abs(a-b)<1e-5;}
static void reticle_mode_tests(){
    auto policy=reticle_draw_policy(ReticleMode::Ingame,true,true,true);
    assert(policy.suppress_native&&policy.draw_native&&!policy.draw_custom);
    policy=reticle_draw_policy(ReticleMode::None,true,true,false);
    assert(policy.suppress_native&&!policy.draw_native&&!policy.draw_custom);
    policy=reticle_draw_policy(ReticleMode::Custom,true,false,true);
    assert(!policy.suppress_native&&!policy.draw_native&&policy.draw_custom);
    for(auto mode:{ReticleMode::Ingame,ReticleMode::Custom,ReticleMode::None}){
        policy=reticle_draw_policy(mode,false,true,true);
        assert(!policy.suppress_native&&!policy.draw_native&&!policy.draw_custom);
        policy=reticle_draw_policy(mode,true,false,false);assert(!policy.suppress_native);
    }
    policy=reticle_draw_policy(ReticleMode(77),true,true,true);
    assert(policy.suppress_native&&policy.draw_native&&!policy.draw_custom);
    const auto mesh=custom_reticle_mesh();
    auto covered=[&](double x,double y,int layer){
        for(size_t i=0;i<mesh.size();i+=3)if(layer<0||mesh[i].layer==layer){
            std::array<Vec3,3> vertices;
            for(unsigned j=0;j<3;j++)vertices[j]={mesh[i+j].x,mesh[i+j].y,-1};
            if(intersect_triangle({{x,y,0},{0,0,-1}},vertices))return true;
        }
        return false;
    };
    for(double x:{-.5,.5})for(double y:{-.5,.5})assert(covered(x,y,2));
    assert(!covered(0,0,-1)&&!covered(0,.5,-1)&&!covered(.5,0,-1));
}
static void reticle_size_tests(){
    // A slower user-selected falloff must visibly retain more artwork at six
    // metres, independently of its minimum angular floor.
    assert(close(reticle_size_reference(6,2,{.05f,4.f}),3));
    assert(close(reticle_size_reference(6,2,{.05f,1.f}),1.2));
    // Defaults shrink monotonically to the calibrated 22% perceived-size floor. These
    // hand-derived ratios include both sides of the floor boundary.
    const double distances[]={2,3,4,6,8,32};
    const double ratios[]={1,.870129870130,.770114942529,.626168224299,.527559055118,.22};
    double previous=2;
    for(unsigned i=0;i<6;i++){
        const double perceived=reticle_size_reference(distances[i],2)/distances[i];
        assert(close(perceived,ratios[i])&&perceived<=previous);previous=perceived;
    }
    // Matching falloff to the reference reproduces original constant physical
    // scaling until the floor, without the removed 1.35 near-size multiplier.
    for(double distance:{.5,1.,2.,4.,8.,20.})assert(close(reticle_size_reference(distance,2,{.05f,2.f}),2));
    assert(close(reticle_size_reference(2,2),2));
    assert(close(reticle_size_reference(22,2,{-3,100}),11)); // .05 minimum, 20m falloff.
    assert(close(reticle_size_reference(22,2,{3,-1}),22)); // 100% floor wins.
    assert(close(reticle_size_reference(22,2,{.05f,-1}),1.1)); // .2m falloff.
    assert(close(reticle_size_reference(4,2,{NAN,NAN}),3.080459770115));
    assert(close(reticle_size_reference(4,2,{.5f,INFINITY}),3.080459770115));
    assert(close(reticle_size_reference(NAN,2),2));
    assert(close(reticle_size_reference(4,NAN),0));
}
static void resources(NativeFrame& f) {
    f.blobs.clear();
    f.blobs.push_back({"texture0",std::vector<uint8_t>(2097152)});
    f.blobs.push_back({"texture1",std::vector<uint8_t>(2097152)});
    f.blobs.push_back({"palette",std::vector<uint8_t>(16384)});
    f.blobs.push_back({"colorxlat",std::vector<uint8_t>(49152)});
    f.blobs.push_back({"luma",std::vector<uint8_t>(32768)});
    f.blobs.push_back({"gamma",std::vector<uint8_t>(256)});
    unsigned base=5|(10<<5)|(15<<10);f.blobs[2].bytes[8192]=base;f.blobs[2].bytes[8193]=base>>8;
    for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)
        f.blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
    for(unsigned i=0;i<32768;i++)f.blobs[4].bytes[i]=(i&127)/2;
    for(unsigned i=0;i<256;i++)f.blobs[5].bytes[i]=255-i;
}
static NativeFrame frame() {
    NativeFrame f;f.width=496;f.height=384;f.yoffset=127;
    f.lens={496,384,400.,400.,248,191};f.complete=true;resources(f);return f;
}
static NativePolygon quad(float depth=100) {
    NativePolygon p;p.count=4;p.focus={1200,1200};p.center={248,320};p.viewport={0,128,496,512};
    p.header={0,0,0,0};p.luma=252;
    p.vertices={NativeVertex{-10000,10000,depth,0,0},NativeVertex{10000,10000,depth,8,0},
        NativeVertex{10000,-10000,depth,8,8},NativeVertex{-10000,-10000,depth,0,8}};return p;
}
#ifdef GUNBLADE_RENDERER_GL_TEST
static void upload_cache_tests() {
    auto fixture=frame();fixture.polygons={quad()};
    const auto count=size_t(fixture.width)*fixture.height;
    fixture.background.assign(count,0xff102030);
    fixture.foreground.assign(count,0);
    fixture.native.assign(count,0xff345678);
    // Read the entire real framebuffer in both eyes. A fresh renderer is the
    // uncached reference for every transition, including native fallback.
    auto render=[](GLScene& scene,const NativeFrame& f) {
        std::vector<uint8_t> result(2*496*384*4);
        const auto& l=f.lens;const float n=.001f;
        const auto projection=glm::frustum(float(-l.cx/l.fx*n),float((f.width-l.cx)/l.fx*n),
            float(-(f.height-l.cy)/l.fy*n),float(l.cy/l.fy*n),n,100.f);
        unsigned eye_index=0;
        for(float eye:{-.032f,.032f}) {
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),.01f,2.f);
            glReadPixels(0,0,496,384,GL_RGBA,GL_UNSIGNED_BYTE,result.data()+eye_index++*496*384*4);
        }
        return result;
    };
    GLScene cached;
    auto verify=[&](const NativeFrame& f,unsigned resources,unsigned rasters) {
        const auto snapshot=std::make_shared<const NativeFrame>(f);
        observed_resource_uploads=observed_raster_uploads=0;
        original_tex_image_2d=glad_glTexImage2D;glad_glTexImage2D=observe_tex_image_2d;
        cached.prepare(snapshot);glad_glTexImage2D=original_tex_image_2d;
        if(observed_resource_uploads!=resources||observed_raster_uploads!=rasters)
            std::cerr<<"Upload mismatch: resources "<<observed_resource_uploads<<" expected "<<resources
                <<", rasters "<<observed_raster_uploads<<" expected "<<rasters<<'\n';
        assert(observed_resource_uploads==resources&&observed_raster_uploads==rasters);
        assert(cached.prepared_frame()==snapshot);
        const auto actual=render(cached,f);
        {GLScene reference;reference.prepare(snapshot);assert(actual==render(reference,f));}
        assert(glGetError()==GL_NO_ERROR);
        return actual;
    };
    const auto original=verify(fixture,6,3);
    const std::weak_ptr<const NativeFrame> old_snapshot=cached.prepared_frame();
    ++fixture.sequence;fixture.time+=1./60;
    assert(original==verify(fixture,0,0)); // Distinct immutable snapshot, identical bytes.
    assert(old_snapshot.expired()); // The cache does not retain previous full snapshots.
    auto geometry=fixture;geometry.polygons[0].luma=128;
    assert(original!=verify(geometry,0,0));verify(fixture,0,0); // Material/geometry still rebuilds.
    for(unsigned n=0;n<6;n++) {
        fixture.blobs[n].bytes.back()^=1;
        verify(fixture,1,0); // Every resource is compared, including unused bytes.
    }
    const auto before_palette=verify(fixture,0,0);
    fixture.blobs[2].bytes[8192]^=31;
    assert(before_palette!=verify(fixture,1,0)); // A live palette change reaches both eyes.
    for(unsigned n=0;n<3;n++) {
        auto& raster=n==0?fixture.background:n==1?fixture.foreground:fixture.native;
        raster[191*fixture.width+248]=0xff12ab34;
        verify(fixture,0,1);
    }
    // Exact source length matters even when the added bytes are zero padding;
    // crossing a resource row boundary also changes its GPU texture shape.
    fixture.blobs[5].bytes.resize(257);verify(fixture,1,0);
    fixture.blobs[5].bytes.resize(2049);verify(fixture,1,0);
    fixture.blobs[0].bytes.resize(1048576);fixture.blobs[1].bytes.resize(1048576);verify(fixture,2,0);
    // The same raster bytes in a different width/height need new definitions.
    fixture.width=248;fixture.height=768;fixture.lens={248,768,400,400,124,383};verify(fixture,0,3);
    fixture.width=496;fixture.height=384;fixture.lens={496,384,400,400,248,191};verify(fixture,0,3);
    auto fallback=fixture;fallback.rejected=1;verify(fallback,0,0);
    fallback.native[191*fallback.width+248]=0xffabcdef;verify(fallback,0,1);
    auto missing=fixture;missing.blobs.clear();verify(missing,0,1);
    auto invalid=fixture;invalid.blobs[5].bytes.resize(255);verify(invalid,0,0);
    verify(fixture,0,0); // Missing/invalid material never invalidates unchanged GPU content.
    auto no_rasters=fixture;no_rasters.background.clear();no_rasters.foreground.clear();no_rasters.native.clear();verify(no_rasters,0,0);
    auto malformed=fixture;malformed.background.pop_back();malformed.foreground.pop_back();malformed.native.pop_back();verify(malformed,0,0);
    verify(fixture,0,0); // Restoring valid byte-identical rasters can reuse their definitions.
    // Zero is opaque black for background/native, but transparent foreground.
    auto zeros=fixture;zeros.background.assign(count,0);zeros.foreground.assign(count,0);zeros.native.assign(count,0);
    verify(zeros,0,3);zeros.rejected=1;verify(zeros,0,0);
    auto incomplete=fixture;incomplete.complete=false;verify(incomplete,0,0);
    verify(fixture,0,3);
    cached.prepare({});verify(fixture,0,0);
    std::cout<<"Model 2 exact-byte upload cache: distinct snapshots, all6 resources/all3 rasters, shape/alpha/fallback and stereo equivalence passed\n";
}
#endif
static NativePolygon camera_quad(double yaw_degrees=0,double distance=2,double half_width=.25) {
    auto p=quad();p.focus={400,400};
    const double yaw=yaw_degrees*3.14159265358979323846/180,s=std::sin(yaw),c=std::cos(yaw);
    const double xs[]={-half_width,half_width,half_width,-half_width},ys[]={half_width,half_width,-half_width,-half_width};
    for(unsigned i=0;i<4;i++) {
        // The fixture is a physical square facing the specified gaze direction.
        const double x=s*distance+c*xs[i],z=-c*distance+s*xs[i];
        p.vertices[i]={float(x*40000),float(ys[i]*40000),float(-z*100),float(i==1||i==2?64:0),float(i>=2?64:0)};
    }
    return p;
}
// Synthetic geometry carries the audited Revision A source/material identity;
// the original game's polygon artwork is deliberately absent from this test.
static std::vector<NativePolygon> reticle_polygons(unsigned player,float x=0,float y=0) {
    std::vector<NativePolygon> result;
    for(unsigned i=0;i<30;i++) {
        auto p=quad(10);p.attr=132609;p.window=4;p.focus={600,600};p.luma=255;
        if(i>=28){p.count=3;p.attr=i==28?132354:132610;}
        if(i<3||(i>=19&&i<24)){p.attr=132097;p.culled=true;}
        p.matrix={.08f,0,0,0,.08f,0,0,0,.08f,x,y,10};
        p.object=i<19?(player?0x80cbe7:0x80cb22):(player?0x80cde6:0x80ccac);
        p.header={192,0,0,i<19?(player?11584u:384u):(player?11648u:10496u)};
        // All synthetic pieces overlap in a 20-pixel square about the native
        // gun point. Its 19+11 list structure is independent of screen position.
        for(auto& v:p.vertices){v.x=v.x*.01f+x*600;v.y=v.y*.01f+y*600;}
        result.push_back(p);
    }
    return result;
}
static void native_reticle_identity_tests() {
    auto f=frame();f.polygons=reticle_polygons(0,.1f,.2f);
    auto p2=reticle_polygons(1,-.1f,-.2f);f.polygons.insert(f.polygons.end(),p2.begin(),p2.end());
    auto hud=quad(10);hud.window=4;hud.focus={600,600};hud.header={192,0,0,384};f.polygons.push_back(hud);
    NativeReticleDiagnostics diagnostics;
    auto sources=native_reticles(f,&diagnostics);
    assert(diagnostics.polygon_count==61&&diagnostics.unknown_focus600_polygons==1);
    assert(diagnostics.players[0].reason_flags==0&&diagnostics.players[0].known_polygons==30);
    assert(diagnostics.players[0].selected_polygons==30&&diagnostics.players[0].visible_polygons==22);
    assert(sources[0].valid()&&sources[1].valid());
    assert(sources[0].polygons.size()==30&&sources[1].polygons.size()==30);
    assert(close(sources[0].center_x,254)&&close(sources[0].center_y,179));
    for(const auto& source:sources)for(size_t index:source.polygons)assert(index<60);
    // Real captured frames submit the same lists in windows 3 and 4, including
    // eight culled pieces per player. Only the complete exact duplicate can be
    // suppressed together with the top-window source. Cull state is preserved.
    auto both=f;for(auto p:f.polygons)if(p.object!=0){p.window=3;both.polygons.push_back(p);}
    sources=native_reticles(both);
    assert(sources[0].valid()&&sources[1].valid());
    for(const auto& source:sources)assert(source.polygons.size()==30&&source.duplicates.size()==60);
    // Native repeated window submissions differ by up to two rotation float
    // ULPs. The 24-bit command quantizer also changes near-cancelled vertices;
    // these actual captured differences are far below one raster pixel.
    auto rounded=both;
    for(size_t i=80;i<91;i++)for(int step=0;step<2;step++)rounded.polygons[i].matrix[0]=std::nextafter(rounded.polygons[i].matrix[0],1.f);
    rounded.polygons[4].vertices[0].y=-.114574432f;rounded.polygons[65].vertices[0].y=-.114568710f;
    rounded.polygons[65].vertices[0].x+=.0078125f;
    sources=native_reticles(rounded);
    assert(sources[0].valid()&&sources[0].duplicates.size()==60&&sources[1].valid());
    rounded.polygons[80].matrix[0]=std::nextafter(rounded.polygons[80].matrix[0],1.f);
    assert(!native_reticles(rounded)[0].valid()); // More than the measured two ULPs.
    rounded=both;rounded.polygons[65].vertices[0].x+=.11f;
    assert(!native_reticles(rounded)[0].valid()); // More than .01 source pixel.
    rounded=both;rounded.polygons[65].vertices[0].x+=.09f;
    assert(native_reticles(rounded)[0].valid()); // .009 source pixel is within it.
    rounded.polygons[65].vertices[0].x=NAN;assert(!native_reticles(rounded)[0].valid());
    rounded=both;rounded.polygons[80].matrix[0]=NAN;assert(!native_reticles(rounded)[0].valid());
    both.polygons[61].vertices[0].x+=1;
    assert(!native_reticles(both)[0].valid()&&native_reticles(both)[1].valid());
    both.polygons[0].count=both.polygons[61].count=5;
    assert(!native_reticles(both)[0].valid()); // Corrupt direct snapshots are bounded.
    assert(f.polygons[0].culled&&native_reticles(f)[0].valid());
    // A partial native list cannot authorize hiding the remaining artwork.
    f.polygons.erase(f.polygons.begin());sources=native_reticles(f);
    assert(!sources[0].valid()&&sources[1].valid());
    f.polygons=reticle_polygons(0);f.polygons[0].header[3]=11584;
    assert(!native_reticles(f,&diagnostics)[0].valid());
    assert((diagnostics.players[0].reason_flags&ReticleMaterialMismatch)!=0);
    assert((diagnostics.players[1].reason_flags&ReticleNoKnownSource)!=0);
    f.polygons=reticle_polygons(0);f.polygons[0].focus={400,400};assert(!native_reticles(f)[0].valid());
    f.polygons=reticle_polygons(0);f.polygons[0].window=3;assert(!native_reticles(f)[0].valid());
    f.polygons=reticle_polygons(0);f.polygons[0].matrix[9]=1;assert(!native_reticles(f)[0].valid());
    f.polygons=reticle_polygons(0);f.polygons[0].vertices[0].z=9;assert(!native_reticles(f)[0].valid());
    f.polygons=reticle_polygons(0);f.complete=false;assert(!native_reticles(f)[0].valid());
    f.complete=true;f.rejected=1;assert(!native_reticles(f)[0].valid());
    // Native window numbers are submission counters. The actual Quest area-two
    // capture moves complete sources to window2, then2/3. Selection follows
    // complete audited source identity and highest equivalent submission.
    for(unsigned top:{2u,3u,5u}){
        f=frame();f.polygons=reticle_polygons(0);for(auto& polygon:f.polygons)polygon.window=top;
        auto lower=f.polygons;for(auto& polygon:lower)polygon.window=top-1;
        f.polygons.insert(f.polygons.end(),lower.begin(),lower.end());
        sources=native_reticles(f,&diagnostics);
        assert(sources[0].valid()&&sources[0].window==top&&sources[0].duplicates.size()==60);
        assert(diagnostics.players[0].selected_window==top&&diagnostics.players[0].reason_flags==0);
    }
    f=frame();f.polygons=reticle_polygons(0);for(auto& polygon:f.polygons)polygon.window=5;
    sources=native_reticles(f,&diagnostics);
    assert(diagnostics.players[0].known_polygons==30&&diagnostics.players[0].windows[5]==30);
    assert(sources[0].valid()&&sources[0].window==5&&sources[0].duplicates.size()==30);
    auto partial=f.polygons;partial.resize(15);for(auto& polygon:partial)polygon.window=6;
    f.polygons.insert(f.polygons.end(),partial.begin(),partial.end());
    assert(!native_reticles(f)[0].valid()); // Partial higher source never hides native artwork.
    f=frame();f.polygons=reticle_polygons(0);f.polygons[0].matrix[0]=.1f;
    native_reticles(f,&diagnostics);assert((diagnostics.players[0].reason_flags&ReticleTransformMismatch)!=0);
    f=frame();f.polygons=reticle_polygons(0);f.polygons[0].matrix[9]=1;
    native_reticles(f,&diagnostics);assert((diagnostics.players[0].reason_flags&ReticleOriginMismatch)!=0);
    f=frame();f.polygons=reticle_polygons(0);f.polygons[0].focus={400,400};
    native_reticles(f,&diagnostics);assert((diagnostics.players[0].reason_flags&ReticleFocusMismatch)!=0);
    f=frame();f.polygons=reticle_polygons(0);f.rejected=1;
    native_reticles(f,&diagnostics);assert(diagnostics.frame_rejected==1&&(diagnostics.players[0].reason_flags&ReticleFrameRejected)!=0);
}
static void immersive_geometry_tests() {
    auto f=frame();auto p=quad();
    // Captured X/Y already contain this polygon's focus. Recover camera units
    // before applying the uniform world scale, independent of the frame lens.
    auto positions=world_positions(p,f,.01f,2.f,true);
    assert(close(positions[0].x,-1./12)&&close(positions[0].y,1./12)&&close(positions[0].z,-1));
    auto world=camera_quad(),reticle=world;reticle.focus={600,600};f.polygons={reticle,reticle,world};
    const auto lens=native_camera_lens(f);
    assert(close(lens.fx,400)&&close(lens.fy,400)&&close(lens.cx,248)&&close(lens.cy,191));
    f.polygons.clear();assert(close(native_camera_lens(f).fx,400));
    for(double yaw:{-60.,60.,90.,180.}) {
        p=camera_quad(yaw);f.polygons={p};
        auto triangles=scene_triangles(f,false,true);assert(triangles.size()==2);
        const double angle=yaw*3.14159265358979323846/180;
        auto hit=trace_scene_aim({{0,0,0},{std::sin(angle),0,-std::cos(angle)}},f,.01f,3.f,false,nullptr,true);
        assert(hit.target&&close(hit.target->x,2*std::sin(angle))&&close(hit.target->z,-2*std::cos(angle)));
        if(yaw==180)assert(scene_triangles(f,true).empty()&&!hit.native.valid);
    }
    // A world subwindow cannot restore the original screen crop in immersive
    // mode; panel primitives retain their native clip and diagnostic behavior.
    p=camera_quad();p.viewport={248,128,496,512};f.polygons={p};
    auto triangles=scene_triangles(f,false,true);assert(triangles.size()==2);
    assert(triangles[0].vertices[0].pixel_x<248);
    p.culled=true;f.polygons={p};assert(scene_triangles(f,true,true).empty());
    // Gunblade currently captures only a cull boolean: its rejected primitives
    // stay suppressed until this game's culling provenance is actually audited.
    p=camera_quad();f.polygons={p};
    auto hit=trace_scene_aim({{.032,0,0},{0,0,-1}},f,.01f,3.f,false,nullptr,true);
    assert(hit.target&&close(hit.target->x,.032)&&close(hit.target->z,-2));
    auto far=camera_quad(0,4);far.window=2;f.polygons={p,far};
    hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,3.f,false,nullptr,true);
    assert(hit.target&&close(hit.target->z,-4)); // Preserve native window priority.
    // Signed geometry must also retain material occupancy instead of losing all
    // textured fragments because the native positive-Z scanline ring is empty.
    p=camera_quad(180);p.header[0]=0x6000;p.lod=10000;f.polygons={p};
    for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0x22);
    hit=trace_scene_aim({{0,0,0},{0,0,1}},f,.01f,3.f,false,nullptr,true);assert(hit.target&&close(hit.target->z,2));
    for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0xff);
    assert(!trace_scene_aim({{0,0,0},{0,0,1}},f,.01f,3.f,false,nullptr,true).target);
}
int main() {
    reticle_mode_tests();
    reticle_size_tests();
    native_reticle_identity_tests();
    immersive_geometry_tests();
    auto f=frame();auto p=quad();
    auto model2b=f;
    model2b.blobs[0].bytes.resize(1048576);model2b.blobs[1].bytes.resize(1048576);
    assert(Model2Resources(model2b).valid()); // Actual Model 2B mapped bank sizes.
    // A focus/window change must preserve its native pixels under one common VR lens.
    auto vertices=world_positions(p,f,.01f,2.f);
    auto a=presentation_lens(f).project_point(vertices[0]);
    assert(close(a.u,148./495)&&close(a.v,91./383));
    assert(close(vertices[0].z,-1));
    f.polygons={p};
    auto hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-1));
    assert(hit.native.valid&&hit.native.inside&&close(hit.native.u,248./495));
    // Shared triangle clipping cannot show or aim through a native viewport border.
    f.polygons[0].viewport={248,128,496,512};
    auto clipped=scene_triangles(f,false);assert(!clipped.empty());
    for(const auto& t:clipped)for(const auto& v:t.vertices)assert(v.pixel_x>=248-1e-5);
    for(const auto& t:scene_triangles(f,true))for(const auto& v:t.vertices)assert(v.pixel_x>=248-1e-5);
    f.polygons[0].culled=true;assert(scene_triangles(f,true).empty());
    // Offscreen native geometry becomes visible only when the requested wider view is enabled.
    f.polygons={p};f.polygons[0].vertices[0].x=-100000;
    auto normal=scene_triangles(f,false),wide=scene_triangles(f,true);
    bool outside=false;for(const auto& t:wide)for(const auto& v:t.vertices)outside|=v.pixel_x<0;
    assert(outside);for(const auto& t:normal)for(const auto& v:t.vertices)assert(v.pixel_x>=0);
    // Native override order, rather than a conventional physical Z buffer, selects the visible surface.
    auto near=quad(50),far=quad(150);near.sort=10;far.sort=20;far.window=2;
    f.polygons={near,far};hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-1.5));
    // Screen-space direct polygons remain at the selected panel distance.
    p.focus={1,1};f.polygons={p};vertices=world_positions(p,f,.01f,3.f);
    for(auto v:vertices)assert(close(v.z,-3));
    // No geometry, including boot/service screens, still has a usable native aiming panel.
    f.polygons.clear();hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-2)&&hit.native.valid);
    // Model 2 packs four nibble texels per little-endian word in 2x2 blocks.
    f.blobs[0].bytes[0]=0x34;f.blobs[0].bytes[1]=0x12;Model2Resources material(f);
    assert(material.texel(0,0,0,0,0)==1);assert(material.texel(0,0,0,1,0)==2);
    assert(material.texel(0,0,0,0,1)==3);assert(material.texel(0,0,0,1,1)==4);
    // Transparent solid polygons do not hide world geometry or consume an aim hit.
    p=quad();p.header[0]=0x2000;f.polygons={p};
    assert(scene_triangles(f,false).empty());
    // These literals independently exercise palette -> color translator -> gamma.
    p.header[0]=0;assert(material.sample(p,0,0,100,248,191)==0xffac9884u);
    std::fill(f.blobs[0].bytes.begin(),f.blobs[0].bytes.end(),0x22);
    std::fill(f.blobs[1].bytes.begin(),f.blobs[1].bytes.end(),0x22);
    p.header[0]=0x4000;p.lod=10000;
    assert(material.sample(p,1,1,100,248,191)==0xffe4d0bcu);
    p.header[0]=0xc000;assert(!material.sample(p,1,1,100,249,191));
    // Alpha key 15 must expose the panel, not hide it with a colored polygon.
    std::fill(f.blobs[0].bytes.begin(),f.blobs[0].bytes.end(),0xff);
    std::fill(f.blobs[1].bytes.begin(),f.blobs[1].bytes.end(),0xff);
    p.header[0]=0x6000;f.polygons={p};assert(!material.sample(p,1,1,100,248,191));
    hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);assert(hit.target&&close(hit.target->z,-2));
    // A comfortable display FOV changes presentation, while its aim still maps
    // to the original native pixel. Native camera focus is not relabeled.
    f=frame();f.polygons={quad()};auto display_lens=f.lens;display_lens.fx=display_lens.fy=390;
    hit=trace_scene_aim({{.032,0,0},{0,0,-1}},f,.01f,2.f,false,&display_lens);
    assert(hit.target&&close(hit.target->z,-1)&&close(hit.native.u,(248+390*.032)/495));
    // Checker coverage uses floor, including negative native coordinates exposed
    // by wider view. Truncating -.5 to zero would select the opposite pixel.
    p=quad();p.header[0]=0x8000;for(unsigned i=0;i<4;i++)p.vertices[i].x*=10;
    f.polygons={p};hit=trace_scene_aim({{0,0,0},{(-.5-248)/f.lens.fx,0,-1}},f,.01f,2.f,true);
    assert(hit.target&&close(hit.target->z,-2));
    // Incomplete frames are never shown and must not produce an actionable aim.
    f.complete=false;assert(!trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f).target);
    // A missing resource causes the renderer's native panel fallback; aim follows it.
    f.complete=true;f.blobs.clear();f.polygons={quad()};
    hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);assert(hit.target&&close(hit.target->z,-2));
    // A transparent quad's source scanline texel, rather than its triangulation,
    // determines whether a real 3D aim hit reaches the surface or the panel.
    f=frame();p=quad();p.header[0]=0x6000;p.lod=10000;
    p.vertices[0].u=0;p.vertices[0].v=0;p.vertices[1].u=64;p.vertices[1].v=0;
    p.vertices[2].u=640;p.vertices[2].v=640;p.vertices[3].u=0;p.vertices[3].v=64;
    f.polygons={p};
    auto source=native_polygon_raster(f,p);auto q=native_polygon_q(source,248.5f,191.5f);
    assert(q&&close((*q)[1]/(*q)[0],22.20045)&&close((*q)[2]/(*q)[0],22.20045));
    for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1") {
        std::fill(blob.bytes.begin(),blob.bytes.end(),0xff);
        for(int y=20;y<25;y++)for(int x=20;x<25;x++) {
            const int word=(y/2)*512+x/2,shift=(!(y&1)?8:0)+(!(x&1)?4:0);
            auto& byte=blob.bytes[word*2+shift/8];byte=uint8_t((byte&~(15<<(shift&7)))|(2<<(shift&7)));
        }
    }
    const Ray quad_ray{{0,0,0},{.5/400,-.5/400,-1}};
    hit=trace_scene_aim(quad_ray,f,.01f,2.f);assert(hit.target&&close(hit.target->z,-1));
    for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0xff);
    hit=trace_scene_aim(quad_ray,f,.01f,2.f);assert(hit.target&&close(hit.target->z,-2));
#ifdef GUNBLADE_RENDERER_GL_TEST
    // Real GPU color checks: no raster or polygon material is mocked here.
    GLContext context;f=frame();f.polygons={quad()};f.background.assign(size_t(f.width)*f.height,0xff000000);
    GLuint framebuffer,color,depth;glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,f.width,f.height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,f.width,f.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glViewport(0,0,f.width,f.height);auto l=f.lens;const float n=.001f;
    auto projection=glm::frustum(float(-l.cx/l.fx*n),float((f.width-l.cx)/l.fx*n),float(-(f.height-l.cy)/l.fy*n),float(l.cy/l.fy*n),n,100.f);
    auto pixel=[&](int x,int y){uint8_t p[4];glReadPixels(x,f.height-1-y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return 0xff000000u|(unsigned(p[0])<<16)|(unsigned(p[1])<<8)|p[2];};
    upload_cache_tests();
    {
        // A target two metres away replaces only a positively identified native
        // ring. Unknown window-4/focus-600 HUD remains at its original location.
        GLScene scene;f.polygons=reticle_polygons(0,1);
        auto p2=reticle_polygons(1,-1);f.polygons.insert(f.polygons.end(),p2.begin(),p2.end());
        auto duplicates=f.polygons;for(auto p:duplicates){p.window=3;f.polygons.push_back(p);}
        auto hud=quad(10);hud.window=4;hud.focus={600,600};
        for(auto& v:hud.vertices){v.x=v.x*.005f+1520;v.y*=.005f;}
        f.polygons.push_back(hud);scene.prepare(std::make_shared<NativeFrame>(f));
        assert(scene.prepared_frame()&&scene.reticle_diagnostics().scene_usable);
        assert(scene.reticle_diagnostics().players[0].suppressed_polygons==60);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u&&pixel(248,191)==0xff000000u);
        // A native ring is visual feedback, never the scene surface selected by
        // the gun ray. Both exact windows must be removed from cached aiming.
        auto raw_hit=trace_scene_aim({{0,0,0},{.15,0,-1}},f,.01f,3.f);
        auto scene_hit=scene.trace_aim({{0,0,0},{.15,0,-1}},.01f,3.f);
        assert(raw_hit.target&&close(raw_hit.target->z,-.1));
        assert(scene_hit.target&&close(scene_hit.target->z,-3));
        scene.set_reticle_targets({Vec3{0,0,-2},std::nullopt},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(248,191)==0xffc0c0c0u&&pixel(308,191)==0xff000000u);
        assert(pixel(257,191)==0xffc0c0c0u&&pixel(260,191)==0xff000000u); // Original near size, no 1.35 boost.
        assert(pixel(400,191)==0xffac9884u);
        assert(pixel(188,191)==0xffc0c0c0u); // P2 has no replacement target.
        scene.set_immersive(true);scene.prepare(std::make_shared<NativeFrame>(f),true);
        auto hud_capture=[&](float size){scene.set_hud_size(size,true);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);std::vector<uint8_t> pixels(496*384*4);glReadPixels(0,0,496,384,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
        auto ring100=hud_capture(1);assert(ring100==hud_capture(.5f)&&ring100==hud_capture(2));
        auto unchanged_hit=scene.trace_aim({{0,0,0},{.15,0,-1}},.01f,3.f);assert(unchanged_hit.target&&close(unchanged_hit.target->z,-3));
        scene.set_hud_size(1,false);scene.set_immersive(false);scene.prepare(std::make_shared<NativeFrame>(f));
        // The same shared marker yields 12.8 pixels of stereo disparity for a
        // 64-mm eye separation at two metres, never an eye-centered overlay.
        for(float eye:{-.032f,.032f}) {
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),.01f,2.f);
            const int center=eye<0?254:241;
            assert(pixel(center,191)==0xffc0c0c0u);
            assert(pixel(eye<0?237:258,191)==0xff000000u);
        }
        // No target (menu/service/unfocused/untracked) and nonfinite targets
        // retain the native artwork; a rejected scene retains the exact raster.
        scene.set_reticle_targets({},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u&&pixel(248,191)==0xff000000u);
        scene.set_reticle_targets({Vec3{NAN,0,-2},std::nullopt},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u);
        // This explicit 25% / 1.8 m reference profile reaches its angular
        // floor at eight metres while preserving stereo disparity.
        scene.set_reticle_size(.25f,1.8f);
        scene.set_reticle_targets({Vec3{0,0,-8},std::nullopt},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(249,191)==0xffc0c0c0u&&pixel(251,191)==0xff000000u);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(.032f,0,0)),.01f,2.f);
        assert(pixel(251,191)==0xffc0c0c0u&&pixel(244,191)==0xff000000u);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,2.f);
        assert(pixel(251,191)==0xff000000u&&pixel(244,191)==0xffc0c0c0u);
        // A larger selected minimum changes artwork size in both eye passes,
        // with the same shared eight-metre target rather than an eye overlay.
        scene.set_reticle_size(.75f,1.8f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(.032f,0,0)),.01f,2.f);
        assert(pixel(256,191)==0xffc0c0c0u&&pixel(239,191)==0xff000000u);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,2.f);
        assert(pixel(256,191)==0xff000000u&&pixel(239,191)==0xffc0c0c0u);
        scene.set_reticle_targets({Vec3{0,0,-6},std::nullopt},glm::mat4(1));
        scene.set_reticle_size(.05f,1.f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(252,191)==0xff000000u);
        scene.set_reticle_size(.05f,4.f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(252,191)==0xffc0c0c0u); // Larger distance shrinks more slowly.
        scene.set_reticle_size(.25f,1.8f);
        // The head supplies a shared orientation; its translation must not
        // move the target. A rolled rectangular ring swaps its width/height.
        auto rectangle=f;
        for(auto& polygon:rectangle.polygons)if(polygon.object==0x80cb22||polygon.object==0x80ccac)
            for(auto& vertex:polygon.vertices)vertex.y*=.5f;
        scene.prepare(std::make_shared<NativeFrame>(rectangle));
        auto head=glm::rotate(glm::mat4(1),float(3.14159265358979323846/2),glm::vec3(0,0,1));head[3]=glm::vec4(0,0,-4,1);
        scene.set_reticle_targets({Vec3{0,0,-2},std::nullopt},head);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(248,199)==0xffc0c0c0u&&pixel(256,191)==0xff000000u);
        // Artwork size uses native angular size at the chosen reference panel
        // distance, independently of the geometry's original tenth-metre Z.
        scene.prepare(std::make_shared<NativeFrame>(f));scene.set_reticle_targets({Vec3{0,0,-2},std::nullopt},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,4.f);
        assert(pixel(263,191)==0xffc0c0c0u&&pixel(278,191)==0xff000000u);
        // A malformed source list keeps its native ring even with a valid aim
        // target; no fallback dot or broad matching-HUD mask is invented.
        auto unknown=f;unknown.polygons[0].header[3]=11584;scene.prepare(std::make_shared<NativeFrame>(unknown));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u&&pixel(248,191)==0xff000000u);
        // None suppresses identified native sources even without tracked
        // targets. The same selection cannot suppress menu/service artwork.
        auto shifted=f;for(auto& polygon:shifted.polygons)polygon.window-=2;
        scene.prepare(std::make_shared<NativeFrame>(shifted));scene.set_reticle_targets({},glm::mat4(1));
        scene.set_reticle_mode(ReticleMode::None,true);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xff000000u&&pixel(188,191)==0xff000000u&&pixel(400,191)==0xffac9884u);
        scene.set_reticle_mode(ReticleMode::None,false);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u&&pixel(188,191)==0xffc0c0c0u);
        scene.set_reticle_mode(ReticleMode::Custom,true);scene.set_reticle_targets({Vec3{0,0,-2},std::nullopt},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xff000000u&&pixel(188,191)==0xff000000u&&pixel(400,191)==0xffac9884u);
        assert(pixel(248,191)==0xff000000u&&pixel(262,177)==0xffff2020u);
        scene.set_reticle_mode(ReticleMode::Custom,false);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u&&pixel(262,177)==0xff000000u);
        // A foreign/partial source is never masked by a mode choice.
        scene.prepare(std::make_shared<NativeFrame>(unknown));scene.set_reticle_mode(ReticleMode::None,true);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(308,191)==0xffc0c0c0u);
        f.rejected=1;f.native.assign(size_t(f.width)*f.height,0xff123456);
        scene.prepare(std::make_shared<NativeFrame>(f));scene.set_reticle_targets({Vec3{0,0,-2},std::nullopt},glm::mat4(1));
        scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xff123456u);
        assert(!scene.reticle_diagnostics().scene_usable&&scene.reticle_diagnostics().frame_rejected==1);
    }
    {
        // Custom does not depend on native reticle artwork or world resource
        // availability. Its four colored arms and every outline leave the
        // exact aim point empty in each eye, over the original native fallback.
        auto custom=frame();custom.polygons.clear();custom.blobs.clear();
        custom.native.assign(size_t(custom.width)*custom.height,0xff102030);
        GLScene scene;scene.prepare(std::make_shared<NativeFrame>(custom));
        scene.set_reticle_mode(ReticleMode::Custom,true);
        scene.set_reticle_targets({Vec3{0,0,-2},Vec3{.35,0,-2}},glm::mat4(1));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(248,191)==0xff102030u&&pixel(318,191)==0xff102030u);
        assert(pixel(262,177)==0xffff2020u&&pixel(332,177)==0xff2066ffu);
        for(float eye:{-.032f,.032f}){
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),.01f,2.f);
            assert(pixel(eye<0?254:241,191)==0xff102030u);
            assert(pixel(eye<0?268:256,177)==0xffff2020u);
        }
        scene.set_reticle_targets({Vec3{0,0,-8},std::nullopt},glm::mat4(1));scene.set_reticle_size(.25f,1.8f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(251,187)==0xffff2020u&&pixel(258,180)==0xff102030u&&pixel(248,191)==0xff102030u);
        scene.set_reticle_size(.75f,1.8f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(258,180)==0xffff2020u&&pixel(248,191)==0xff102030u);
        scene.set_reticle_mode(ReticleMode::Custom,false);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(262,177)==0xff102030u);
        scene.set_reticle_mode(ReticleMode::None,true);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(262,177)==0xff102030u&&pixel(248,191)==0xff102030u);
    }
    f=frame();f.polygons={quad()};f.background.assign(size_t(f.width)*f.height,0xff000000);
    {GLScene scene;auto image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xffac9884u);
        // Cached aiming consumes the very snapshot and clipped triangle stream
        // prepared for drawing, with identical two-hand/material semantics.
        auto compare_aim=[](const SceneAim& a,const SceneAim& b){
            assert(bool(a.target)==bool(b.target)&&a.native.valid==b.native.valid&&a.native.inside==b.native.inside);
            if(a.target)assert(close(a.target->x,b.target->x)&&close(a.target->y,b.target->y)&&close(a.target->z,b.target->z));
            if(a.native.valid)assert(close(a.native.u,b.native.u)&&close(a.native.v,b.native.v));
        };
        {
            GLScene cached;assert(!cached.trace_aim({{0,0,0},{0,0,-1}},.01f,3.f).target);
            NativeLens override_lens=f.lens;override_lens.fx=override_lens.fy=390;
            const Ray hands[]={{{-.032,0,0},{0,0,-1}},{{.032,0,0},{0,0,-1}},{{0,0,0},{.0025,0,-1}},{{0,0,0},{1,0,-1}}};
            auto verify=[&](const NativeFrame& fixture,bool wider,bool immersive){
                auto snapshot=std::make_shared<const NativeFrame>(fixture);cached.prepare(snapshot,wider);cached.set_immersive(immersive);cached.set_presentation_lens(override_lens);
                for(const auto& ray:hands)for(int repeat=0;repeat<2;repeat++)compare_aim(cached.trace_aim(ray,.01f,3.f),trace_scene_aim(ray,*snapshot,.01f,3.f,wider,&override_lens,immersive));
            };
            auto fixture=frame();fixture.polygons={quad()};verify(fixture,false,false);
            auto cached_hit=cached.trace_aim(hands[0],.01f,3.f);assert(cached_hit.target&&close(cached_hit.target->z,-1));
            auto changed_lens=override_lens;changed_lens.fx=changed_lens.fy=300;cached.set_presentation_lens(changed_lens);
            compare_aim(cached.trace_aim(hands[1],.01f,3.f),trace_scene_aim(hands[1],fixture,.01f,3.f,false,&changed_lens));
            fixture.polygons={quad(50)};verify(fixture,false,false);cached_hit=cached.trace_aim(hands[1],.01f,3.f);assert(cached_hit.target&&close(cached_hit.target->z,-.5));
            fixture.polygons={quad()};fixture.polygons[0].vertices[0].x=-100000;verify(fixture,true,false);verify(fixture,false,false);
            auto wide_fixture=frame();wide_fixture.polygons={quad()};for(auto& v:wide_fixture.polygons[0].vertices)v.x*=10;
            const auto wide_snapshot=std::make_shared<const NativeFrame>(wide_fixture);const Ray outside{{0,0,0},{-1,0,-1}};
            cached.prepare(wide_snapshot,false);cached_hit=cached.trace_aim(outside,.01f,3.f);assert(cached_hit.target&&close(cached_hit.target->z,-3));
            cached.prepare(wide_snapshot,true);cached_hit=cached.trace_aim(outside,.01f,3.f);assert(cached_hit.target&&close(cached_hit.target->z,-1));
            cached.prepare(wide_snapshot,false);cached_hit=cached.trace_aim(outside,.01f,3.f);assert(cached_hit.target&&close(cached_hit.target->z,-3));
            fixture.polygons={camera_quad(60)};verify(fixture,false,true);verify(fixture,false,false);
            fixture.polygons={quad()};fixture.polygons[0].header[0]=0x8000;verify(fixture,false,false);
            fixture.polygons[0].header[0]=0x6000;fixture.polygons[0].lod=10000;
            for(auto& blob:fixture.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0xff);
            verify(fixture,false,false);cached_hit=cached.trace_aim(hands[0],.01f,3.f);assert(cached_hit.target&&close(cached_hit.target->z,-3));
            fixture=frame();fixture.polygons={quad()};fixture.polygons[0].focus={1,1};verify(fixture,false,false);
            fixture.polygons[0].focus={400,400};fixture.rejected=1;verify(fixture,false,false);
            fixture.rejected=0;fixture.blobs.clear();verify(fixture,true,false);
            fixture.complete=false;verify(fixture,false,false);assert(!cached.trace_aim(hands[0],.01f,3.f).target);
            cached.prepare({});assert(!cached.trace_aim(hands[0],.01f,3.f).target);
        }
        for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0x22);
        f.polygons[0].header[0]=0x4000;f.polygons[0].lod=10000;image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xffe4d0bcu);
        f.polygons[0].header[0]=0xc000;image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);
        assert(pixel(249,191)==0xff000000u);assert(pixel(248,191)==0xffe4d0bcu);
        // Moving the eye shifts the same real surface and its ray hit together.
        f.polygons[0].header[0]=0x4000;image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,2.f);
        // Gunblade fx400 at1m shifts the native surface by12.8pixels.
        assert(pixel(134,191)==0xff000000u);assert(pixel(136,191)==0xffe4d0bcu);assert(pixel(337,191)==0xff000000u);
        auto aim=trace_scene_aim({{.032,0,0},{0,0,-1}},f,.01f,2.f);assert(aim.target&&close(aim.target->z,-1));
        assert(close(aim.native.u,(248+400.*.032)/495));
        scene.set_presentation_lens(display_lens);
        auto display_projection=glm::frustum(float(-display_lens.cx/display_lens.fx*n),float((f.width-display_lens.cx)/display_lens.fx*n),float(-(f.height-display_lens.cy)/display_lens.fy*n),float(display_lens.cy/display_lens.fy*n),n,100.f);
        scene.draw(display_projection,.01f,2.f);assert(pixel(248,191)==0xffe4d0bcu);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(display_projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,2.f);
        // fx390 at1m gives12.48pixels ofparallax, not58px orpanel6.24px.
        assert(pixel(134,191)==0xff000000u);assert(pixel(137,191)==0xffe4d0bcu);assert(pixel(337,191)==0xff000000u);
        scene.set_presentation_lens({});
        // Native quads interpolate along their left/right scanline edges. A
        // diagonal triangle fan changes non-planar texture coordinates even
        // when the quad's four positions lie on one physical plane.
        f=frame();f.polygons={quad()};auto& warped=f.polygons[0];
        warped.header[0]=0x4000;warped.lod=10000;
        warped.vertices[0].u=0;warped.vertices[0].v=0;
        warped.vertices[1].u=64;warped.vertices[1].v=0;
        warped.vertices[2].u=640;warped.vertices[2].v=640;
        warped.vertices[3].u=0;warped.vertices[3].v=64;
        for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1") {
            std::fill(blob.bytes.begin(),blob.bytes.end(),0);
            for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
                const int word=(y/2)*512+x/2,shift=(!(y&1)?8:0)+(!(x&1)?4:0);
                blob.bytes[word*2+shift/8]|=uint8_t((1+x/4)<<(shift&7));
            }
        }
        const float fx=(248.5f-148.f)/200.f,fy=(191.5f-91.f)/200.f;
        const auto expected_quad=Model2Resources(f).sample(warped,fx*(8+72*fy),fy*(8+72*fx),100,248,191);
        assert(expected_quad);
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(248,191)==*expected_quad);
        // One ordered geometry submission must retain each primitive's own
        // material, alpha key, checker state and world/panel projection. Calls
        // are observed while still executing the real GPU draw and readback.
        f=frame();f.background.assign(size_t(f.width)*f.height,0xff000000);
        f.foreground.assign(size_t(f.width)*f.height,0);
        f.foreground[190*f.width+310]=0xff12ab34;
        auto back=quad(),middle=quad(),front=quad();
        back.sort=30;middle.sort=20;front.sort=10;
        middle.focus={1,1};middle.header[0]=0x8000;middle.luma=128;
        front.header={0x6000,1,0,64};front.luma=196;front.lod=10000;
        front.vertices[1].u=front.vertices[2].u=256;
        front.vertices[2].v=front.vertices[3].v=256;
        f.polygons={back,middle,front};
        const unsigned alternate=25|(4<<5)|(12<<10);
        f.blobs[2].bytes[8194]=uint8_t(alternate);f.blobs[2].bytes[8195]=uint8_t(alternate>>8);
        for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1") {
            std::fill(blob.bytes.begin(),blob.bytes.end(),0xff);
            for(int y=0;y<32;y++)for(int x=0;x<16;x++) {
                const int word=(y/2)*512+x/2,shift=(!(y&1)?8:0)+(!(x&1)?4:0);
                auto& value=blob.bytes[word*2+shift/8];value=uint8_t((value&~(15<<(shift&7)))|(2<<(shift&7)));
            }
        }
        Model2Resources mixed_material(f);
        const auto front_color=mixed_material.sample(front,5,16,100,180,191);
        const auto middle_color=mixed_material.sample(middle,0,0,100,300,191);
        const auto back_color=mixed_material.sample(back,0,0,100,301,191);
        assert(front_color&&middle_color&&back_color&&front_color!=middle_color&&middle_color!=back_color);
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        original_draw_arrays=glad_glDrawArrays;glad_glDrawArrays=observe_draw_arrays;observed_draw_calls=observed_world_draw_calls=0;
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        glad_glDrawArrays=original_draw_arrays;
        assert(pixel(180,191)==*front_color&&pixel(300,191)==*middle_color&&pixel(301,191)==*back_color);
        assert(pixel(310,190)==0xff12ab34&&pixel(100,191)==0xff000000);
        assert(observed_world_draw_calls==1&&observed_draw_calls==3); // Background, geometry, foreground.
        observed_draw_calls=observed_world_draw_calls=0;glad_glDrawArrays=observe_draw_arrays;
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-.016f,0,0)),.01f,2.f);
        glad_glDrawArrays=original_draw_arrays;
        assert(pixel(180,191)==*front_color&&pixel(300,191)==*back_color&&pixel(301,191)==*middle_color);
        assert(pixel(307,190)==0xff12ab34&&observed_world_draw_calls==1&&observed_draw_calls==3);
        // A rejected scene must show its exact native fallback even if its
        // remaining polygons and material banks would otherwise be drawable.
        f=frame();f.polygons={quad()};f.rejected=1;
        f.native.assign(size_t(f.width)*f.height,0xff123456);
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(248,191)==0xff123456);
        // Missing material resources use the exact native diagnostic raster.
        f.blobs.clear();f.native.assign(size_t(f.width)*f.height,0xff123456);image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xff123456);
        // A focus-600 primitive must use its actual physical coordinates when
        // the eye turns 60 degrees, independent of the focus-400 world lens.
        f=frame();f.background.assign(size_t(f.width)*f.height,0xff000000);
        auto side=camera_quad(60);side.focus={600,600};for(auto& v:side.vertices){v.x*=1.5f;v.y*=1.5f;}
        f.polygons={side};image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.set_immersive(true);
        const auto side_view=glm::lookAt(glm::vec3(0),glm::vec3(std::sqrt(.75f),0,-.5f),glm::vec3(0,1,0));
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*side_view,.01f,3.f);
        assert(pixel(248,191)==0xffac9884u);
        // Real eye rotations reveal side, camera-plane-crossing and behind
        // geometry. Texture alpha remains evaluated on each actual surface.
        for(double yaw:{-60.,60.,90.,180.}) {
            f=frame();f.background.assign(size_t(f.width)*f.height,0xff000000);
            p=camera_quad(yaw);p.header[0]=0x4000;p.lod=10000;f.polygons={p};
            for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0x22);
            const float radians=float(yaw*3.14159265358979323846/180);
            const auto view=glm::lookAt(glm::vec3(0),glm::vec3(std::sin(radians),0,-std::cos(radians)),glm::vec3(0,1,0));
            image=std::make_shared<NativeFrame>(f);scene.prepare(image);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*view,.01f,3.f);
            assert(pixel(248,191)==0xffe4d0bcu&&pixel(150,191)==0xff000000u);
            auto aim=trace_scene_aim({{0,0,0},{std::sin(radians),0,-std::cos(radians)}},f,.01f,3.f,false,nullptr,true);
            assert(aim.target);
        }
        // Lean parallax follows the physical two-metre square: 6.4 source
        // pixels for 32 mm. A presentation lens override cannot stretch it.
        f=frame();f.background.assign(size_t(f.width)*f.height,0xff000000);f.polygons={camera_quad()};
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.set_presentation_lens({496,384,100,100,248,191});
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,3.f);
        assert(pixel(190,191)==0xff000000u&&pixel(193,191)==0xffac9884u&&pixel(293,191)==0xff000000u);
        // Background/foreground rasters do not write an opaque depth plane.
        // Native window priorities still win in the single ordered draw.
        auto near_world=camera_quad(),far_world=camera_quad(0,4);far_world.window=2;far_world.luma=128;
        f.polygons={near_world,far_world};f.foreground.assign(size_t(f.width)*f.height,0);f.foreground[191*f.width+248]=0xff12ab34;
        const auto far_color=Model2Resources(f).sample(far_world,0,0,400,249,191);assert(far_color);
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        observed_draw_calls=observed_world_draw_calls=0;glad_glDrawArrays=observe_draw_arrays;
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,3.f);glad_glDrawArrays=original_draw_arrays;
        assert(pixel(249,191)==*far_color&&pixel(248,191)==0xff12ab34);
        assert(observed_world_draw_calls==1&&observed_draw_calls==3);
        float background_depth=0;glReadPixels(100,f.height-1-191,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&background_depth);assert(close(background_depth,1));
        // Keep Gunblade's native scanline interpolation for a warped quad's
        // UVs; replacing the atlas with triangle UVs changes this center texel.
        f=frame();p=quad();p.focus={400,400};p.header[0]=0x4000;p.lod=10000;
        p.vertices[1].u=64;p.vertices[2].u=640;p.vertices[2].v=640;p.vertices[3].v=64;f.polygons={p};
        for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1") {
            std::fill(blob.bytes.begin(),blob.bytes.end(),0);
            for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
                const int word=(y/2)*512+x/2,shift=(!(y&1)?8:0)+(!(x&1)?4:0);
                blob.bytes[word*2+shift/8]|=uint8_t((1+x/4)<<(shift&7));
            }
        }
        const auto native_quad_color=Model2Resources(f).sample(p,22.20045f,22.20045f,100,248,191);assert(native_quad_color);
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,3.f);
        assert(pixel(248,191)==*native_quad_color);
        // Turning the prototype off on the same snapshot rebuilds the original
        // clipped stream and restores the untouched diagnostic lens override.
        f=frame();f.polygons={camera_quad(60)};f.background.assign(size_t(f.width)*f.height,0xff000000);
        image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.set_presentation_lens({});scene.set_immersive(false);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*side_view,.01f,3.f);assert(pixel(248,191)==0xff000000u);
    }
    assert(glGetError()==GL_NO_ERROR);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    {
        // A new GL context/renderer must populate its own GPU textures even
        // when the prior context displayed exactly the same native bytes.
        GLContext recreated;
        auto restored=frame();restored.polygons={quad()};
        const auto pixels=size_t(restored.width)*restored.height;
        restored.background.assign(pixels,0);restored.foreground.assign(pixels,0);restored.native.assign(pixels,0);
        GLScene scene;observed_resource_uploads=observed_raster_uploads=0;
        original_tex_image_2d=glad_glTexImage2D;glad_glTexImage2D=observe_tex_image_2d;
        scene.prepare(std::make_shared<const NativeFrame>(restored));glad_glTexImage2D=original_tex_image_2d;
        assert(observed_resource_uploads==6&&observed_raster_uploads==3&&glGetError()==GL_NO_ERROR);
    }
    std::cout<<"Model 2 production OpenGL material/stereo/fallback tests passed\n";
#endif
    f=frame();f.polygons={quad()};f.rejected=1;
    hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-2));
    std::cout<<"Model 2 renderer geometry/aim/texture tests passed\n";
}
