// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/quest_foveation.h"
#include <cassert>
#include <cstring>

using gunblade::QuestFoveation;
namespace {
const auto instance=reinterpret_cast<XrInstance>(uintptr_t(1));
const auto session=reinterpret_cast<XrSession>(uintptr_t(2));
const std::array<XrSwapchain,2> eyes{
    reinterpret_cast<XrSwapchain>(uintptr_t(3)),reinterpret_cast<XrSwapchain>(uintptr_t(4))};
bool missing_function{},create_fails{},create_loss_pending{},second_eye_fails{},destroy_fails{},profile_live{};
unsigned updates{};
XrFoveationLevelFB requested_level{};XrFoveationDynamicFB requested_dynamic{};float requested_offset{};
void reset_api(){missing_function=create_fails=create_loss_pending=second_eye_fails=destroy_fails=profile_live=false;updates=0;}
XrResult XRAPI_CALL create_profile(XrSession given,const XrFoveationProfileCreateInfoFB* info,XrFoveationProfileFB* profile){
    assert(given==session&&info->type==XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB);
    const auto* level=static_cast<const XrFoveationLevelProfileCreateInfoFB*>(info->next);
    assert(level&&level->type==XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB&&level->next==nullptr);
    requested_level=level->level;requested_dynamic=level->dynamic;requested_offset=level->verticalOffset;
    if(create_fails)return XR_ERROR_RUNTIME_FAILURE;
    profile_live=true;*profile=reinterpret_cast<XrFoveationProfileFB>(uintptr_t(5));return create_loss_pending?XR_SESSION_LOSS_PENDING:XR_SUCCESS;
}
XrResult XRAPI_CALL update_swapchain(XrSwapchain eye,const XrSwapchainStateBaseHeaderFB* header){
    assert(profile_live&&(eye==eyes[0]||eye==eyes[1]));
    const auto* state=reinterpret_cast<const XrSwapchainStateFoveationFB*>(header);
    assert(state->type==XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB&&state->next==nullptr&&state->flags==0&&state->profile);
    ++updates;return second_eye_fails&&eye==eyes[1]?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_CALL destroy_profile(XrFoveationProfileFB profile){
    assert(profile_live&&profile);profile_live=false;return destroy_fails?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_CALL load(XrInstance given,const char* name,PFN_xrVoidFunction* function){
    assert(given==instance);*function=nullptr;
    if(std::strcmp(name,"xrCreateFoveationProfileFB")==0)*function=reinterpret_cast<PFN_xrVoidFunction>(create_profile);
    if(std::strcmp(name,"xrDestroyFoveationProfileFB")==0)*function=reinterpret_cast<PFN_xrVoidFunction>(destroy_profile);
    if(std::strcmp(name,"xrUpdateSwapchainFB")==0&&!missing_function)*function=reinterpret_cast<PFN_xrVoidFunction>(update_swapchain);
    return *function?XR_SUCCESS:XR_ERROR_FUNCTION_UNSUPPORTED;
}
std::array<XrExtensionProperties,3> supported(){
    std::array<XrExtensionProperties,3> result{};
    const char* names[]={XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,XR_FB_FOVEATION_EXTENSION_NAME,XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME};
    for(unsigned i=0;i<3;++i){result[i].type=XR_TYPE_EXTENSION_PROPERTIES;std::strcpy(result[i].extensionName,names[i]);result[i].extensionVersion=1;}
    return result;
}
QuestFoveation enabled(){
    QuestFoveation foveation;std::vector<const char*> extensions;
    assert(foveation.negotiate(true,true,supported(),extensions));
    assert(foveation.load(instance,load));return foveation;
}
}
static void optional_capabilities_do_not_change_pcvr_or_an_unrequested_build(){
    for(auto gates:{std::array<bool,2>{false,true},std::array<bool,2>{true,false}}){
        QuestFoveation foveation;std::vector<const char*> extensions{"existing"};
        assert(!foveation.negotiate(gates[0],gates[1],supported(),extensions));
        XrSwapchainCreateInfo swapchain{};XrSwapchainCreateInfoFoveationFB chain{};
        assert(!foveation.chain(swapchain,chain)&&swapchain.next==nullptr&&extensions.size()==1);
    }
    for(unsigned missing=0;missing<3;++missing){
        auto advertised=supported();advertised[missing].extensionName[0]='\0';
        QuestFoveation foveation;std::vector<const char*> extensions;
        assert(!foveation.negotiate(true,true,advertised,extensions)&&extensions.empty());
    }
}
static void supported_medium_dynamic_keeps_the_original_render_dimensions(){
    reset_api();auto foveation=enabled();
    int existing_next{};XrSwapchainCreateInfo swapchain{};swapchain.next=&existing_next;
    swapchain.width=2064;swapchain.height=2208;swapchain.sampleCount=1;swapchain.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    XrSwapchainCreateInfoFoveationFB chain{};
    assert(foveation.chain(swapchain,chain)&&swapchain.next==&chain&&chain.next==&existing_next&&chain.flags==0);
    assert(swapchain.width==2064&&swapchain.height==2208&&swapchain.sampleCount==1);
    const auto result=foveation.apply_medium_dynamic(session,eyes);
    assert(result.applied&&!result.recreate_plain_swapchains&&result.result==XR_SUCCESS);
    assert(requested_level==XR_FOVEATION_LEVEL_MEDIUM_FB&&requested_dynamic==XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB&&requested_offset==0);
    assert(updates==2&&!profile_live);
}
static void missing_functions_leave_the_plain_swapchain_path_available(){
    reset_api();missing_function=true;QuestFoveation foveation;std::vector<const char*> extensions;
    assert(foveation.negotiate(true,true,supported(),extensions));assert(!foveation.load(instance,load));
    XrSwapchainCreateInfo swapchain{};XrSwapchainCreateInfoFoveationFB chain{};
    assert(!foveation.chain(swapchain,chain)&&swapchain.next==nullptr);
    assert(!foveation.apply_medium_dynamic(session,eyes).applied&&updates==0);
}
static void failed_profile_or_partial_stereo_update_requires_plain_recreation(){
    for(unsigned failure=0;failure<3;++failure){
        reset_api();auto foveation=enabled();
        create_fails=failure==0;second_eye_fails=failure==1;destroy_fails=failure==2;
        const auto result=foveation.apply_medium_dynamic(session,eyes);
        assert(!result.applied&&result.recreate_plain_swapchains&&result.result==XR_ERROR_RUNTIME_FAILURE);
        assert(!profile_live);
        XrSwapchainCreateInfo swapchain{};XrSwapchainCreateInfoFoveationFB chain{};
        assert(!foveation.chain(swapchain,chain));
    }
}
static void optional_creation_or_session_loss_can_abandon_the_candidate(){
    reset_api();auto foveation=enabled();foveation.abandon();
    XrSwapchainCreateInfo swapchain{};XrSwapchainCreateInfoFoveationFB chain{};
    assert(!foveation.chain(swapchain,chain)&&swapchain.next==nullptr);
    assert(!foveation.load(instance,load));
    foveation=enabled();create_loss_pending=true;
    const auto result=foveation.apply_medium_dynamic(session,eyes);
    assert(!result.applied&&result.recreate_plain_swapchains&&result.result==XR_SESSION_LOSS_PENDING);
    assert(!profile_live&&updates==0);
}
int main(){
    optional_capabilities_do_not_change_pcvr_or_an_unrequested_build();
    supported_medium_dynamic_keeps_the_original_render_dimensions();
    missing_functions_leave_the_plain_swapchain_path_available();
    failed_profile_or_partial_stereo_update_requires_plain_recreation();
    optional_creation_or_session_loss_can_abandon_the_candidate();
}
