// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <openxr/openxr.h>
#include <array>
#include <cstring>
#include <span>
#include <vector>
namespace gunblade {
// Optional Quest/GLES proof only. This header changes nothing until a caller
// explicitly negotiates, loads, chains and applies it. PCVR passes quest_gles=false.
// Khronos XR_FB_foveation + configuration define the profile API. Meta's GLES
// XrCompositor_NativeActivity sample uses the same zero-flags swapchain chain
// and an ordinary depth renderbuffer. No Vulkan fragment-density-map API is used.
// https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_FB_foveation.html
class QuestFoveation {
    bool negotiated_{},ready_{};
    PFN_xrCreateFoveationProfileFB create_{};
    PFN_xrDestroyFoveationProfileFB destroy_{};
    PFN_xrUpdateSwapchainFB update_{};
    void disable() noexcept {ready_=false;create_=nullptr;destroy_=nullptr;update_=nullptr;}
public:
    inline static constexpr std::array<const char*,3> RequiredExtensions{
        XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,XR_FB_FOVEATION_EXTENSION_NAME,
        XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME};
    struct ApplyResult {bool applied{},recreate_plain_swapchains{};XrResult result{XR_SUCCESS};};
    // Use if optional swapchain creation fails, then recreate any earlier eye
    // chains through the original path before rendering either eye.
    void abandon() noexcept {disable();negotiated_=false;}
    // Invoke before xrCreateInstance. All optional dependencies must be advertised;
    // partial support adds no extensions and retains the original rendering path.
    bool negotiate(bool quest_gles,bool requested,std::span<const XrExtensionProperties> supported,
                   std::vector<const char*>& extensions) {
        disable();negotiated_=false;
        if(!quest_gles||!requested)return false;
        for(const char* required:RequiredExtensions) {
            bool found=false;
            for(const auto& extension:supported)if(std::strcmp(extension.extensionName,required)==0){found=true;break;}
            if(!found)return false;
        }
        for(const char* required:RequiredExtensions) {
            bool present=false;
            for(const char* enabled:extensions)if(enabled&&std::strcmp(enabled,required)==0){present=true;break;}
            if(!present)extensions.push_back(required);
        }
        negotiated_=true;return true;
    }
    // Load after instance creation, before requesting any foveated swapchain.
    bool load(XrInstance instance,PFN_xrGetInstanceProcAddr get) noexcept {
        disable();if(!negotiated_||!instance||!get)return false;
        PFN_xrVoidFunction create{},destroy{},update{};
        if(get(instance,"xrCreateFoveationProfileFB",&create)!=XR_SUCCESS||!create||
           get(instance,"xrDestroyFoveationProfileFB",&destroy)!=XR_SUCCESS||!destroy||
           get(instance,"xrUpdateSwapchainFB",&update)!=XR_SUCCESS||!update)return false;
        create_=reinterpret_cast<PFN_xrCreateFoveationProfileFB>(create);
        destroy_=reinterpret_cast<PFN_xrDestroyFoveationProfileFB>(destroy);
        update_=reinterpret_cast<PFN_xrUpdateSwapchainFB>(update);ready_=true;return true;
    }
    // Storage must outlive xrCreateSwapchain. On a chained creation failure the
    // caller retries its original create-info without this node. Dimensions,
    // formats, sample count and render resolution are deliberately untouched.
    bool chain(XrSwapchainCreateInfo& swapchain,XrSwapchainCreateInfoFoveationFB& storage) const noexcept {
        if(!ready_)return false;
        storage={};storage.type=XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB;
        storage.next=const_cast<void*>(swapchain.next);swapchain.next=&storage;return true;
    }
    // Apply once to the freshly created stereo chains, requesting Medium with
    // runtime dynamic adjustment. No image-quality or GPU
    // gain is established by successful API calls: those need device A/B proof.
    // Failure can leave one eye configured. Caller MUST destroy/recreate both
    // eye swapchains through its plain path if recreate_plain_swapchains is set.
    ApplyResult apply_medium_dynamic(XrSession session,std::span<const XrSwapchain> swapchains) noexcept {
        if(!ready_)return {false,false,XR_ERROR_FUNCTION_UNSUPPORTED};
        if(!session||swapchains.empty()) {disable();return {false,true,XR_ERROR_HANDLE_INVALID};}
        for(auto swapchain:swapchains)if(!swapchain){disable();return {false,true,XR_ERROR_HANDLE_INVALID};}
        XrFoveationLevelProfileCreateInfoFB level{};level.type=XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB;
        level.level=XR_FOVEATION_LEVEL_MEDIUM_FB;level.dynamic=XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB;
        XrFoveationProfileCreateInfoFB info{};info.type=XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB;info.next=&level;
        XrFoveationProfileFB profile{};XrResult result=create_(session,&info,&profile);
        if(result!=XR_SUCCESS||!profile) {
            if(profile&&XR_SUCCEEDED(result))destroy_(profile);
            disable();return {false,true,result==XR_SUCCESS?XR_ERROR_RUNTIME_FAILURE:result};
        }
        XrSwapchainStateFoveationFB state{};state.type=XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB;state.profile=profile;
        for(auto swapchain:swapchains) {
            result=update_(swapchain,reinterpret_cast<const XrSwapchainStateBaseHeaderFB*>(&state));
            if(result!=XR_SUCCESS)break;
        }
        // Khronos explicitly permits destroying a profile after it is applied;
        // swapchains retain their parameters. Cleanup failures are surfaced.
        const auto cleanup=destroy_(profile);
        if(result==XR_SUCCESS&&cleanup!=XR_SUCCESS)result=cleanup;
        if(result!=XR_SUCCESS){disable();return {false,true,result};}
        return {true,false,XR_SUCCESS};
    }
};
}
