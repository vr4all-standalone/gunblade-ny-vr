// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/raster_policy.h"
#include <cassert>
#include <cstdlib>
#include <new>
using namespace gunblade;
static size_t allocated_bytes;
void* operator new(size_t size) {
    allocated_bytes+=size;
    if(void* value=std::malloc(size?size:1))return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept {std::free(value);}
void operator delete(void* value,size_t) noexcept {std::free(value);}
static NativeFrame healthy_frame() {
    NativeFrame frame;frame.width=496;frame.height=384;frame.yoffset=128;
    frame.background.resize(496*384);frame.complete=false; // Pre-raster owned capture.
    constexpr const char* names[]={"texture0","texture1","palette","colorxlat","luma","gamma"};
    constexpr unsigned sizes[]={1048576,1048576,16384,49152,32768,256};
    for(int i=0;i<6;i++)frame.blobs.push_back({names[i],std::vector<uint8_t>(sizes[i])});
    NativePolygon polygon;polygon.count=3;polygon.window=1;polygon.focus={400,400};
    polygon.center={248,320};polygon.viewport={0,128,496,512};
    polygon.vertices={NativeVertex{-100,100,10,0,0},NativeVertex{100,100,10,0,0},NativeVertex{0,-100,10,0,0},NativeVertex{}};
    frame.polygons.push_back(polygon);return frame;
}
static RasterDecision full_scene_reference(const NativeFrame& frame) {
    for(const auto& triangle:scene_triangles(frame,false)) {
        if(triangle.panel||frame.polygons[triangle.polygon].window!=1)continue;
        const auto& a=triangle.vertices[0];const auto& b=triangle.vertices[1];const auto& c=triangle.vertices[2];
        const double area=(b.pixel_x-a.pixel_x)*(c.pixel_y-a.pixel_y)-(b.pixel_y-a.pixel_y)*(c.pixel_x-a.pixel_x);
        if(std::isfinite(area)&&std::abs(area)>1e-5)return RasterDecision::Skip;
    }
    return RasterDecision::NoVisibleWorld;
}
static void preflight_matches_full_scene_clipping(const RasterContext& context) {
    auto frame=healthy_frame();
    const auto source=frame.polygons[0];
    // Hand-picked near-plane, viewport, degenerate, panel and hidden cases.
    for(int sample=0;sample<504;sample++) {
        frame.width=sample%2?496:320;frame.height=sample%2?384:240;
        frame.xoffset=(sample%3-1)*97;frame.yoffset=(sample%4-1)*128;
        frame.background.resize(size_t(frame.width)*frame.height);
        frame.lens={frame.width,frame.height,250.+sample,380.+sample,73.,111.};
        auto polygon=source;polygon.focus=sample%7==0?std::array<float,2>{1,1}:
            sample%7==1?std::array<float,2>{600,600}:std::array<float,2>{-400,400};
        polygon.center={float(120+sample%9*31),float(100+sample%5*53)};
        polygon.viewport=sample%3==0?std::array<int,4>{80,180,350,450}:source.viewport;
        switch(sample%9) {
        case 0: polygon.vertices[0].z=-3;break;
        case 1: polygon.vertices[0].z=.00001f;polygon.vertices[1].z=-1;break;
        case 2: for(auto& vertex:polygon.vertices)vertex.x=100000;break;
        case 3: polygon.vertices[2]=polygon.vertices[0];break;
        case 4: for(auto& vertex:polygon.vertices)vertex.z=-1;break;
        case 5: polygon.count=4;polygon.vertices[3]={-150,-80,12,3,7};break;
        case 6: polygon.culled=true;break;
        case 7: polygon.header[0]=1<<13;break;
        default: break;
        }
        // Source order and native sort order deliberately disagree. Only an
        // existence query may ignore sorting; actual drawing still sorts.
        auto other=polygon;other.window=4;other.sort=0;polygon.sort=999;
        frame.polygons={other,polygon};
        NativeFrame thin;thin.width=frame.width;thin.height=frame.height;
        thin.xoffset=frame.xoffset;thin.yoffset=frame.yoffset;thin.lens=frame.lens;
        thin.polygons.push_back(polygon);
        const auto full_triangles=scene_triangles(frame,false);
        const auto thin_triangles=scene_triangles(thin,false);
        size_t candidate_triangle=0;
        for(const auto& triangle:full_triangles)if(triangle.polygon==1) {
            assert(candidate_triangle<thin_triangles.size());
            const auto& reference=thin_triangles[candidate_triangle++];
            assert(triangle.panel==reference.panel);
            for(unsigned vertex=0;vertex<3;vertex++) {
                const auto& a=triangle.vertices[vertex];const auto& b=reference.vertices[vertex];
                assert(a.pixel_x==b.pixel_x && a.pixel_y==b.pixel_y && a.depth==b.depth);
                assert(a.u==b.u && a.v==b.v);
                assert(a.camera.x==b.camera.x && a.camera.y==b.camera.y && a.camera.z==b.camera.z);
                assert(a.native_homogeneous.x==b.native_homogeneous.x &&
                       a.native_homogeneous.y==b.native_homogeneous.y && a.native_homogeneous.z==b.native_homogeneous.z);
            }
        }
        assert(candidate_triangle==thin_triangles.size());
        assert(raster_decision(context,&frame)==full_scene_reference(frame));
    }
    frame=healthy_frame();frame.polygons.clear();
    assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame=healthy_frame();
    auto hidden=source;for(auto& vertex:hidden.vertices)vertex.x=100000;
    frame.polygons.assign(64,hidden);frame.polygons.push_back(source);
    assert(raster_decision(context,&frame)==full_scene_reference(frame));
    assert(raster_decision(context,&frame)==RasterDecision::Skip);
}
static void first_visible_preflight_has_bounded_allocation(const RasterContext& context) {
    auto frame=healthy_frame();frame.polygons.resize(3000,frame.polygons[0]);
    const size_t before=allocated_bytes;
    assert(raster_decision(context,&frame)==RasterDecision::Skip);
    // Catch reconstructing the entire scene or copying multi-megabyte blobs
    // when one eligible polygon already proves that fallback can be skipped.
    assert(allocated_bytes-before<16384);
}
static void observed_area2_world_remains_eligible_when_window1_is_offscreen(const RasterContext& context) {
    auto frame=healthy_frame();
    // Actual Area2 frame6902 has an offscreen focus407 window1 and visible
    // full-viewport focus407 world geometry in window2. Window numbers count
    // native window commands rather than identifying a semantic world layer.
    auto world=frame.polygons[0];world.window=2;world.focus={407,407};
    world.viewport={0,128,498,512};world.vertices[2].z=11;
    auto hidden=world;hidden.window=1;
    for(auto& vertex:hidden.vertices)vertex.x=100000;
    frame.polygons={hidden,world};
    assert(raster_decision(context,&frame)==RasterDecision::Skip);
    // The newly observed world profile must not authorize arbitrary HUDs.
    for(unsigned window:{0u,2u,3u,4u,7u}){
        frame.polygons={world};frame.polygons[0].window=window;
        assert(raster_decision(context,&frame)==RasterDecision::Skip);
    }
    frame.polygons={world};
    frame.polygons[0].focus={600,600};assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame.polygons[0]=world;frame.polygons[0].focus={408,408};assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame.polygons[0]=world;frame.polygons[0].focus={400,400};assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame.polygons[0]=world;frame.polygons[0].viewport={80,180,350,450};assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame.polygons[0]=world;frame.polygons[0].center={247,320};assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame.polygons[0]=world;for(auto& vertex:frame.polygons[0].vertices)vertex.z=10;
    assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
    frame.polygons={world};frame.xoffset=1;assert(raster_decision(context,&frame)==RasterDecision::NoVisibleWorld);
}
int main() {
    auto direct=std::make_unique<NativeFrame>(healthy_frame());
    assert(prepare_direct_framebuffer_frame(direct,true,42,1.25,0,128,496,384)==1);
    assert(direct->polygons.empty());
    auto ordinary=std::make_unique<NativeFrame>(healthy_frame());
    assert(prepare_direct_framebuffer_frame(ordinary,false,42,1.25,0,128,496,384)==1);
    assert(ordinary->polygons.size()==1);
    std::unique_ptr<NativeFrame> absent;
    prepare_direct_framebuffer_frame(absent,false,42,1.25,0,128,496,384);assert(!absent);
    prepare_direct_framebuffer_frame(absent,true,42,1.25,0,128,496,384);
    assert(absent && absent->polygons.empty() && !absent->complete);
    assert(absent->sequence==42 && absent->time==1.25 && absent->width==496 && absent->height==384);
    assert(absent->xoffset==0 && absent->yoffset==128);
    RasterContext context;context.vr=context.requested=context.host_ready=true;
    context.state.valid=context.state.playing=true;context.state.mode=2;context.state.submode=2;
    observed_area2_world_remains_eligible_when_window1_is_offscreen(context);
    preflight_matches_full_scene_clipping(context);
    first_visible_preflight_has_bounded_allocation(context);
    auto frame=healthy_frame();
    assert(raster_decision(context,&frame)==RasterDecision::Skip);
    context.direct_framebuffer=true;assert(raster_decision(context,&frame)==RasterDecision::DirectFramebuffer);context.direct_framebuffer=false;
    context.requested=false;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.requested=true;
    context.vr=false;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.vr=true;
    context.cpu_reference=true;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.cpu_reference=false;
    context.capture=true;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.capture=false;
    context.host_ready=false;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.host_ready=true;
    context.quit=true;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.quit=false;
    context.full_frame=false;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.full_frame=true;
    context.state.service=true;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.state.service=false;
    context.state.submode=6;assert(raster_decision(context,&frame)!=RasterDecision::Skip);context.state.submode=2;
    assert(raster_decision(context,nullptr)!=RasterDecision::Skip);
    frame.rejected=1;assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame.rejected=0;
    frame.blobs[0].bytes.clear();assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    frame.polygons[0].culled=true;assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    frame.polygons[0].window=4;assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    frame.polygons[0].focus={1,1};assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    for(auto& vertex:frame.polygons[0].vertices)vertex.x=100000;
    assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    frame.background.clear();assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    frame.polygons[0].vertices[2]=frame.polygons[0].vertices[0];assert(raster_decision(context,&frame)!=RasterDecision::Skip);frame=healthy_frame();
    for(auto& vertex:frame.polygons[0].vertices)vertex.z=-1;
    assert(raster_decision(context,&frame)!=RasterDecision::Skip);
}
