#include "FabricBackend.h"
#include "AmberDynamicLibrary.h"
#include "fabric/fabric_amber.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <set>
#include <vector>

namespace fabric {
namespace {
using GetApi = AmberResult (AMBER_CALL *)(uint32_t, uint32_t, void *);
FabricResult map(AmberResult result) { return result == AMBER_OK ? FABRIC_OK : FABRIC_BACKEND_ERROR; }

class AmberInstance final : public FabricBackendInstance {
public:
    AmberInstance(std::unique_ptr<AmberDynamicLibrary> module, const AmberApiV2 &api, AmberHandle handle,
                  const FabricLaunchRequest &request) : module_(std::move(module)), api_(api), handle_(handle) {
        for (uint32_t i=0; i<request.rom_resource_count; ++i) {
            const auto &r=request.rom_resources[i];
            if (r.role==FABRIC_ROM_ROLE_PROGRAM) program_.push_back({r.slot,r.path});
            else if (r.role==FABRIC_ROM_ROLE_SOUND) sound_.push_back({r.slot,r.path});
        }
        if (!request.rom_resources) for (uint32_t i=0;i<request.rom_path_count;++i) program_.push_back({i,request.rom_paths[i]});
        auto order=[](auto &v){std::stable_sort(v.begin(),v.end(),[](const auto&a,const auto&b){return a.first<b.first;});}; order(program_); order(sound_);
        if (request.machine_configuration_size) std::memcpy(&config_,request.machine_configuration,sizeof(config_));
    }
    ~AmberInstance() override { release(); if (handle_ && initialised_ && !shutdown_) api_.Shutdown(handle_); if(handle_) api_.Destroy(handle_); }
    FabricResult initialise() noexcept override {
        AmberInitialiseParams p{}; p.struct_size=sizeof(p);
        if(program_.size()>4||sound_.size()>4) return fail("Amber supports at most four program and four sound ROMs",FABRIC_INVALID_ARGUMENT);
        for(size_t i=0;i<program_.size();++i)p.program_roms[i]=program_[i].second.c_str();
        for(size_t i=0;i<sound_.size();++i)p.sound_roms[i]=sound_[i].second.c_str();
        AmberResult r=api_.Initialise(handle_,&p); if(r!=AMBER_OK) return amber(r,"Amber Initialise failed");
        initialised_=true;
        if ((config_.flags&7u) && !apply_config()) return FABRIC_BACKEND_ERROR;
        return FABRIC_OK;
    }
    FabricResult reset() noexcept override { return amber(api_.Reset(handle_),"Amber Reset failed"); }
    FabricResult advance(uint64_t ns) noexcept override {
        accumulator_ += ns*UINT64_C(8000000); uint64_t cycles=accumulator_/UINT64_C(1000000000); accumulator_%=UINT64_C(1000000000);
        while(cycles){uint32_t chunk=static_cast<uint32_t>(std::min<uint64_t>(cycles,UINT32_MAX));int32_t ran=0;AmberResult r=api_.Run(handle_,chunk,&ran);if(r!=AMBER_OK)return amber(r,"Amber Run failed");cycles-=chunk;} return FABRIC_OK;
    }
    FabricResult shutdown() noexcept override { release(); AmberResult r=api_.Shutdown(handle_); if(r==AMBER_OK){shutdown_=true;initialised_=false;} return amber(r,"Amber Shutdown failed"); }
    FabricResult submit_input(const FabricInput &in) noexcept override { if(in.numerical_index<0||static_cast<uint32_t>(in.numerical_index)>=caps_.max_switches)return fail("switch index exceeds Amber maximum",FABRIC_INVALID_ARGUMENT); AmberResult r=api_.SetSwitchState(handle_,static_cast<uint32_t>(in.numerical_index),in.active?1u:0u);if(r==AMBER_OK){if(in.active) asserted_.insert(in.numerical_index);else asserted_.erase(in.numerical_index);}return amber(r,"Amber SetSwitchState failed"); }
    FabricResult capabilities(FabricCapabilities &out) noexcept override { out.flags=0;if(caps_.feature_bits&AMBER_CAP_SWITCH_INPUT)out.flags|=FABRIC_CAPABILITY_DIGITAL_INPUT;if(caps_.feature_bits&AMBER_CAP_OUTPUT_SNAPSHOT)out.flags|=FABRIC_CAPABILITY_LAMPS|FABRIC_CAPABILITY_REELS|FABRIC_CAPABILITY_CHARACTER_DISPLAYS|FABRIC_CAPABILITY_SEGMENT_DISPLAYS;if(caps_.feature_bits&AMBER_CAP_AUDIO)out.flags|=FABRIC_CAPABILITY_AUDIO;return FABRIC_OK; }
    FabricResult snapshot(FabricMachineSnapshot &out) noexcept override {
        AmberOutputSnapshotV1 s{};s.struct_size=sizeof(s);s.version=AMBER_OUTPUT_SNAPSHOT_VERSION_1;AmberResult r=api_.GetOutputSnapshot(handle_,&s);if(r!=AMBER_OK)return amber(r,"Amber GetOutputSnapshot failed");
        out.lamp_count=s.matrix_lamp_count;out.reel_count=s.reel_count;out.character_display_count=s.alpha_display_count;out.segment_display_count=s.seven_segment_display_count;
        if(out.lamp_capacity<out.lamp_count||out.reel_capacity<out.reel_count||out.character_display_capacity<out.character_display_count||out.segment_display_capacity<out.segment_display_count)return FABRIC_BUFFER_TOO_SMALL;
        if((out.lamp_count&&!out.lamps)||(out.reel_count&&!out.reels)||(out.character_display_count&&!out.character_displays)||(out.segment_display_count&&!out.segment_displays))return fail("snapshot output buffer is null",FABRIC_INVALID_ARGUMENT);
        for(uint32_t i=0;i<out.lamp_count;++i){auto &d=out.lamps[i];d={};d.struct_size=sizeof(d);d.struct_version=FABRIC_ABI_VERSION_1;std::snprintf(d.identifier,sizeof(d.identifier),"amber.lamp.%u",i);d.numerical_index=static_cast<int32_t>(i);d.logical_state=s.matrix_lamps[i].is_on?1:0;d.brightness=static_cast<float>(s.matrix_lamps[i].brightness_q16_16)/65536.0f;}
        for(uint32_t i=0;i<out.reel_count;++i){auto &d=out.reels[i];d={};d.struct_size=sizeof(d);d.struct_version=FABRIC_ABI_VERSION_1;std::snprintf(d.identifier,sizeof(d.identifier),"amber.reel.%u",i);d.numerical_index=static_cast<int32_t>(i);d.position=s.reel_positions[i];}
        for(uint32_t i=0;i<out.character_display_count;++i){auto &d=out.character_displays[i];d={};d.struct_size=sizeof(d);d.struct_version=FABRIC_ABI_VERSION_1;std::snprintf(d.identifier,sizeof(d.identifier),"amber.alpha.%u",i);d.character_count=AMBER_ALPHA_CHARACTERS;d.character_capacity=FABRIC_CHARACTER_CAPACITY;for(uint32_t j=0;j<AMBER_ALPHA_CHARACTERS;++j){d.characters[j]=s.alpha_displays[i].segment_masks[j];d.attributes[j]=s.alpha_displays[i].dot_comma[j];}}
        for(uint32_t i=0;i<out.segment_display_count;++i){auto &d=out.segment_displays[i];d={};d.struct_size=sizeof(d);d.struct_version=FABRIC_ABI_VERSION_1;std::snprintf(d.identifier,sizeof(d.identifier),"amber.seven-segment.%u",i);d.digit_count=1;d.digit_capacity=FABRIC_SEGMENT_DIGIT_CAPACITY;d.segment_masks[0]=s.seven_segment_displays[i].segment_mask;}
        out.sequence=++sequence_;return FABRIC_OK;
    }
    FabricResult audio_format(FabricAudioFormat &out) noexcept override {AmberAudioFormatV1 f{};f.struct_size=sizeof(f);f.version=1;AmberResult r=api_.GetAudioFormat(handle_,&f);if(r!=AMBER_OK)return amber(r,"Amber GetAudioFormat failed");if(f.sample_format!=AMBER_AUDIO_SAMPLE_PCM_S16||f.interleaving!=AMBER_AUDIO_INTERLEAVED)return fail("unsupported Amber audio format",FABRIC_NOT_SUPPORTED);out.sample_rate=f.sample_rate;out.channel_count=static_cast<uint16_t>(f.channels);out.bits_per_sample=16;out.interleaved=1;out.signed_samples=1;out.little_endian=1;return FABRIC_OK;}
    FabricResult read_audio(int16_t *p,uint32_t n,uint32_t &written) noexcept override {written=0;if(n&&!p)return fail("audio buffer is null",FABRIC_INVALID_ARGUMENT);return amber(api_.FillAudioFrames(handle_,p,n,&written),"Amber FillAudioFrames failed");}
    std::string last_error() const noexcept override{return error_;}
    bool query_caps(){caps_.struct_size=sizeof(caps_);caps_.version=1;return api_.GetCapabilities(handle_,&caps_)==AMBER_OK;}
private:
    FabricResult fail(const char*m,FabricResult r){error_=m;return r;} FabricResult amber(AmberResult r,const char*context){if(r==AMBER_OK){error_.clear();return FABRIC_OK;}char b[512]{};uint32_t needed=0;api_.GetLastError(handle_,b,sizeof(b),&needed);error_=std::string(context)+(b[0]?": ":"")+b;return map(r);}
    bool apply_config(){if(config_.magic!=FABRIC_AMBER_CONFIGURATION_MAGIC||config_.struct_size!=sizeof(config_)||config_.version!=1){error_="malformed Amber backend configuration";return false;}if((config_.flags&FABRIC_AMBER_CONFIGURE_REELS)&&api_.ConfigureReels(handle_,&config_.reels)!=AMBER_OK)return false;if((config_.flags&FABRIC_AMBER_CONFIGURE_COINS)&&api_.ConfigureCoins(handle_,&config_.coins)!=AMBER_OK)return false;if((config_.flags&FABRIC_AMBER_CONFIGURE_PERCENTAGE)&&api_.SetPercentageSwitch(handle_,config_.percentage_switch)!=AMBER_OK)return false;return true;}
    void release(){if(handle_&&!shutdown_)for(int32_t i:asserted_)api_.SetSwitchState(handle_,static_cast<uint32_t>(i),0);asserted_.clear();}
    std::unique_ptr<AmberDynamicLibrary> module_;AmberApiV2 api_{};AmberHandle handle_{};AmberCapabilitiesV1 caps_{};std::vector<std::pair<uint32_t,std::string>>program_,sound_;std::set<int32_t>asserted_;FabricAmberConfigurationV1 config_{};std::string error_;uint64_t accumulator_=0,sequence_=0;bool initialised_=false,shutdown_=false;
};

class Provider final:public FabricBackendProvider {public:bool supports(const std::string&k,const std::string&)const noexcept override{return k=="amber-api-v2";}FabricResult create(const FabricLaunchRequest&r,std::unique_ptr<FabricBackendInstance>&out,std::string&e)noexcept override{try{if(!std::filesystem::path(r.backend_path).is_absolute()){e="Amber backend path must be absolute";return FABRIC_INVALID_ARGUMENT;}for(uint32_t i=0;i<r.rom_resource_count;++i)if(r.rom_resources[i].struct_size<sizeof(FabricRomResource)||r.rom_resources[i].struct_version!=FABRIC_ABI_VERSION_1||!r.rom_resources[i].path){e="malformed typed ROM resource";return FABRIC_INVALID_ARGUMENT;}if(r.machine_configuration_size&&r.machine_configuration_size!=sizeof(FabricAmberConfigurationV1)){e="malformed Amber backend configuration size";return FABRIC_INVALID_ARGUMENT;}auto lib=std::make_unique<AmberDynamicLibrary>();if(!lib->open(r.backend_path,e))return FABRIC_NOT_FOUND;auto get=reinterpret_cast<GetApi>(lib->symbol("AmberGetApi"));if(!get){e="Amber DLL is missing AmberGetApi";return FABRIC_NOT_SUPPORTED;}AmberApiV2 api{};api.struct_size=sizeof(api);api.api_version=2;AmberResult ar=get(2,sizeof(api),&api);if(ar!=AMBER_OK || api.struct_size < sizeof(AmberApiV2) || api.api_version != 2){e="Amber API v2 negotiation failed";return FABRIC_UNSUPPORTED_VERSION;}const void*required[]={reinterpret_cast<void*>(api.GetBridgeInfo),reinterpret_cast<void*>(api.EnumerateCore),reinterpret_cast<void*>(api.Create),reinterpret_cast<void*>(api.Destroy),reinterpret_cast<void*>(api.Initialise),reinterpret_cast<void*>(api.Reset),reinterpret_cast<void*>(api.Run),reinterpret_cast<void*>(api.Shutdown),reinterpret_cast<void*>(api.GetLastError),reinterpret_cast<void*>(api.GetCapabilities),reinterpret_cast<void*>(api.SetSwitchState),reinterpret_cast<void*>(api.GetOutputSnapshot),reinterpret_cast<void*>(api.GetAudioFormat),reinterpret_cast<void*>(api.FillAudioFrames),reinterpret_cast<void*>(api.ConfigureReels),reinterpret_cast<void*>(api.ConfigureCoins),reinterpret_cast<void*>(api.SetPercentageSwitch)};for(auto p:required)if(!p){e="Amber API v2 function table is incomplete";return FABRIC_NOT_SUPPORTED;}bool found=false;for(uint32_t i=0;;++i){AmberCoreInfo c{};c.struct_size=sizeof(c);ar=api.EnumerateCore(i,&c);if(ar==AMBER_NO_MORE_ITEMS)break;if(ar!=AMBER_OK){e="Amber core enumeration failed";return FABRIC_BACKEND_ERROR;}if(c.core_id && std::string(r.machine_identifier)==c.core_id)found=true;}if(!found){e="requested machine is not enumerated by Amber DLL";return FABRIC_NOT_FOUND;}AmberHandle h=nullptr;ar=api.Create(r.machine_identifier,&h);if(ar!=AMBER_OK||!h){e="Amber Create failed";return FABRIC_BACKEND_ERROR;}auto instance=std::make_unique<AmberInstance>(std::move(lib),api,h,r);if(!instance->query_caps()){e="Amber GetCapabilities failed";return FABRIC_BACKEND_ERROR;}out=std::move(instance);return FABRIC_OK;}catch(const std::exception&x){e=x.what();return FABRIC_INTERNAL_ERROR;}}};
}
std::unique_ptr<FabricBackendProvider> MakeAmberBackendProvider(){return std::make_unique<Provider>();}
}
