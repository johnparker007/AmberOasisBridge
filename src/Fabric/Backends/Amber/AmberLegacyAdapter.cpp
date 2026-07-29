#include "AmberLegacyAdapter.h"

#include "AmberDynamicLibrary.h"
#include "fabric/fabric_amber.h"
#include "../../../Cores/JPMSystem6/PA2CoreInterface.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <vector>

namespace fabric {
namespace {
template <typename T> T resolve(AmberDynamicLibrary &library, const char *name) {
  void *symbol = library.symbol(name);
  T result = nullptr;
  static_assert(sizeof(result) == sizeof(symbol), "function pointer size");
  std::memcpy(&result, &symbol, sizeof(result));
  return result;
}

struct LegacyApi {
  uint8_t (*Initialise)() = nullptr;
  uint8_t (*Shutdown)() = nullptr;
  void (*Reset)() = nullptr;
  int32_t (*Run)(uint32_t) = nullptr;
  uint32_t (*LoadROM)(uint8_t *, uint8_t *, uint8_t *, uint8_t *) = nullptr;
  uint32_t (*GetOutputSnapshotSize)() = nullptr;
  uint32_t (*GetOutputSnapshot)(void *, uint32_t) = nullptr;
  void (*TurnSwitchOn)(uint8_t) = nullptr;
  void (*TurnSwitchOff)(uint8_t) = nullptr;
  uint32_t (*LoadSoundROM)(uint8_t *, uint8_t *, uint8_t *, uint8_t *) = nullptr;
  uint32_t (*GetAudioFormat)(PA2_AudioFormat *, uint32_t) = nullptr;
  uint32_t (*FillAudioFrames)(int16_t *, uint32_t) = nullptr;
  void (*SetOptoInvert)(uint8_t, uint8_t) = nullptr;
  void (*SetOptoStart)(uint8_t, uint8_t) = nullptr;
  void (*SetOptoEnd)(uint8_t, uint8_t) = nullptr;
  void (*SetSteps)(uint8_t, uint8_t) = nullptr;
  void (*SetCoinValue)(uint8_t, uint8_t) = nullptr;
  void (*SetCoinEnable)(uint8_t, uint8_t) = nullptr;
  void (*SetLockoutVal)(uint8_t, uint8_t) = nullptr;
  void (*SetLockoutInvert)(uint8_t, uint8_t) = nullptr;
  void (*SetPercent)(uint8_t) = nullptr;
};

class LegacyInstance final : public FabricBackendInstance {
public:
  LegacyInstance(std::unique_ptr<AmberDynamicLibrary> library, LegacyApi api,
                 std::vector<std::string> program, std::vector<std::string> sound,
                 const FabricAmberConfigurationV1 *configuration)
      : library_(std::move(library)), api_(api), program_(std::move(program)),
        sound_(std::move(sound)) {
    if (configuration) config_ = *configuration;
  }
  ~LegacyInstance() override { if (started_ && !stopped_) api_.Shutdown(); }

  FabricResult initialise() noexcept override {
    if (!api_.Initialise()) return fail("production Amber Initialise returned failure");
    started_ = true;
    if (!load_roms(api_.LoadROM, program_, "program")) return FABRIC_NOT_FOUND;
    if (!sound_.empty() && !load_roms(api_.LoadSoundROM, sound_, "sound"))
      return api_.LoadSoundROM ? FABRIC_NOT_FOUND : FABRIC_NOT_SUPPORTED;
    if (!configure()) return FABRIC_NOT_SUPPORTED;
    return ok();
  }
  FabricResult reset() noexcept override { api_.Reset(); return ok(); }
  FabricResult advance(uint64_t ns) noexcept override {
    const uint64_t total = remainder_ + ns;
    uint64_t cycles = total / 125u;
    remainder_ = total % 125u;
    while (cycles) {
      const uint32_t request = static_cast<uint32_t>(std::min<uint64_t>(cycles, INT32_MAX));
      const int32_t ran = api_.Run(request);
      if (ran < 0 || static_cast<uint32_t>(ran) > request)
        return fail("production Amber Run returned an invalid cycle count");
      if (!ran) return fail("production Amber Run made zero progress");
      cycles -= static_cast<uint32_t>(ran);
    }
    return ok();
  }
  FabricResult shutdown() noexcept override {
    release_inputs();
    if (stopped_) return shutdown_result_;
    stopped_ = true;
    if (started_ && !api_.Shutdown())
      return shutdown_result_ = fail("production Amber Shutdown returned failure");
    started_ = false;
    return shutdown_result_ = ok();
  }
  FabricResult submit_input(const FabricInput &input) noexcept override {
    if (input.numerical_index < 0 || input.numerical_index > 255)
      return invalid("production Amber switch index must be in the range 0..255");
    const uint8_t index = static_cast<uint8_t>(input.numerical_index);
    if (input.active) { api_.TurnSwitchOn(index); asserted_.insert(index); }
    else { api_.TurnSwitchOff(index); asserted_.erase(index); }
    return ok();
  }
  FabricResult capabilities(FabricCapabilities &out) noexcept override {
    out.flags = FABRIC_CAPABILITY_DIGITAL_INPUT | FABRIC_CAPABILITY_LAMPS |
                FABRIC_CAPABILITY_REELS | FABRIC_CAPABILITY_CHARACTER_DISPLAYS |
                FABRIC_CAPABILITY_SEGMENT_DISPLAYS;
    if (api_.GetAudioFormat && api_.FillAudioFrames) out.flags |= FABRIC_CAPABILITY_AUDIO;
    return ok();
  }
  FabricResult snapshot(FabricMachineSnapshot &out) noexcept override {
    PA2_OutputSnapshot source{};
    const uint32_t expected = api_.GetOutputSnapshotSize();
    if (expected != sizeof(source)) return fail("production Amber output snapshot layout is incompatible");
    if (api_.GetOutputSnapshot(&source, sizeof(source)) != sizeof(source) ||
        source.SizeBytes != sizeof(source) || source.Version != PA2_OUTPUT_SNAPSHOT_VERSION)
      return fail("production Amber GetOutputSnapshot returned an incompatible snapshot");
    if (source.MatrixLampCount > PA2_MAX_MATRIX_LAMPS || source.ReelCount > PA2_NUM_REELS ||
        source.AlphaSegmentedDisplayCount > PA2_NUM_ALPHA_DISPLAYS || source.LedDisplayCount > PA2_NUM_LED_DISPLAYS)
      return fail("production Amber snapshot contains invalid counts");
    out.lamp_count=source.MatrixLampCount; out.reel_count=source.ReelCount;
    out.character_display_count=source.AlphaSegmentedDisplayCount;
    out.segment_display_count=source.LedDisplayCount;
    if (out.lamp_capacity<out.lamp_count || out.reel_capacity<out.reel_count ||
        out.character_display_capacity<out.character_display_count || out.segment_display_capacity<out.segment_display_count)
      return FABRIC_BUFFER_TOO_SMALL;
    if ((out.lamp_count&&!out.lamps)||(out.reel_count&&!out.reels)||(out.character_display_count&&!out.character_displays)||(out.segment_display_count&&!out.segment_displays))
      return invalid("snapshot output buffer is null");
    for(uint32_t i=0;i<out.lamp_count;++i){ auto &d=out.lamps[i]; d={}; d.struct_size=sizeof(d); d.struct_version=FABRIC_ABI_VERSION_1; std::snprintf(d.identifier,sizeof(d.identifier),"amber.lamp.%u",i); d.numerical_index=static_cast<int32_t>(i); d.logical_state=source.MatrixLamps[i].OnOff?1:0; d.brightness=source.MatrixLamps[i].Brightness; }
    for(uint32_t i=0;i<out.reel_count;++i){ auto &d=out.reels[i]; d={}; d.struct_size=sizeof(d); d.struct_version=FABRIC_ABI_VERSION_1; std::snprintf(d.identifier,sizeof(d.identifier),"amber.reel.%u",i); d.numerical_index=static_cast<int32_t>(i); d.position=source.Reels[i].Position; }
    for(uint32_t i=0;i<out.character_display_count;++i){ auto &d=out.character_displays[i]; d={}; d.struct_size=sizeof(d); d.struct_version=FABRIC_ABI_VERSION_1; std::snprintf(d.identifier,sizeof(d.identifier),"amber.alpha.%u",i); d.character_count=PA2_NUM_ALPHA_CHARS; d.character_capacity=FABRIC_CHARACTER_CAPACITY; for(uint32_t j=0;j<PA2_NUM_ALPHA_CHARS;++j){d.characters[j]=source.AlphaSegmented[i].Segments[j];d.attributes[j]=source.AlphaSegmented[i].DotComma[j];} }
    for(uint32_t i=0;i<out.segment_display_count;++i){ auto &d=out.segment_displays[i]; d={}; d.struct_size=sizeof(d); d.struct_version=FABRIC_ABI_VERSION_1; std::snprintf(d.identifier,sizeof(d.identifier),"amber.led.%u",i); d.digit_count=1; d.digit_capacity=FABRIC_SEGMENT_DIGIT_CAPACITY; d.segment_masks[0]=source.LedDisplays[i].OnOff; }
    out.sequence=++sequence_; return ok();
  }
  FabricResult audio_format(FabricAudioFormat &out) noexcept override {
    if(!api_.GetAudioFormat||!api_.FillAudioFrames) return unsupported("production Amber audio exports are unavailable");
    PA2_AudioFormat f{}; f.SizeBytes=sizeof(f); f.Version=PA2_AUDIO_FORMAT_VERSION;
    if(api_.GetAudioFormat(&f,sizeof(f))!=sizeof(f) || f.SizeBytes!=sizeof(f) || f.Version!=PA2_AUDIO_FORMAT_VERSION)
      return fail("production Amber returned an incompatible audio format");
    if(f.Format!=PA2_AUDIO_FORMAT_PCM_S16 || !f.Channels || f.Channels>UINT16_MAX || f.BitsPerSample!=16)
      return unsupported("production Amber audio is not representable interleaved PCM16");
    out.sample_rate=f.SampleRate; out.channel_count=static_cast<uint16_t>(f.Channels); out.bits_per_sample=16; out.interleaved=out.signed_samples=out.little_endian=1; return ok();
  }
  FabricResult read_audio(int16_t *samples,uint32_t capacity,uint32_t &written) noexcept override {
    written=0; if(!api_.FillAudioFrames||!api_.GetAudioFormat) return unsupported("production Amber audio exports are unavailable");
    if(capacity&&!samples) return invalid("audio buffer is null");
    written=api_.FillAudioFrames(samples,capacity);
    if(written>capacity){written=0;return fail("production Amber FillAudioFrames over-reported frame count");}
    return ok();
  }
  std::string last_error() const noexcept override { return error_; }
private:
  using Load = uint32_t (*)(uint8_t*,uint8_t*,uint8_t*,uint8_t*);
  bool load_roms(Load load,const std::vector<std::string>& paths,const char *role){
    if(paths.empty()) return true;
    if(!load){error_=std::string("production Amber has no ")+role+" ROM loader";return false;}
    uint8_t *p[4]{}; for(size_t i=0;i<paths.size();++i)p[i]=reinterpret_cast<uint8_t*>(const_cast<char*>(paths[i].c_str()));
    if(!load(p[0],p[1],p[2],p[3])){error_=std::string("production Amber failed to load ")+role+" ROM resources";return false;} return true;
  }
  bool configure(){
    if(config_.flags&FABRIC_AMBER_CONFIGURE_REELS){ if(!api_.SetSteps||!api_.SetOptoInvert||!api_.SetOptoStart||!api_.SetOptoEnd){error_="production Amber reel configuration exports are unavailable";return false;} for(uint32_t i=0;i<AMBER_MAX_REELS;++i)if(config_.reels.apply_mask&(1u<<i)){const auto&r=config_.reels.reels[i]; if(r.steps>255||r.opto_start>255||r.opto_end>255||r.opto_invert>255){error_="production Amber reel configuration value exceeds 8-bit ABI";return false;} const auto index=static_cast<uint8_t>(i);api_.SetSteps(index,static_cast<uint8_t>(r.steps));api_.SetOptoStart(index,static_cast<uint8_t>(r.opto_start));api_.SetOptoEnd(index,static_cast<uint8_t>(r.opto_end));api_.SetOptoInvert(index,static_cast<uint8_t>(r.opto_invert));} }
    if(config_.flags&FABRIC_AMBER_CONFIGURE_COINS){ if(!api_.SetCoinValue||!api_.SetCoinEnable){error_="production Amber coin configuration exports are unavailable";return false;} for(uint32_t i=0;i<AMBER_MAX_COIN_CHANNELS;++i)if(config_.coins.channel_apply_mask&(1u<<i)){const auto&c=config_.coins.channels[i];if(c.value>255||c.enabled>255||c.lockout_invert>255){error_="production Amber coin configuration value exceeds 8-bit ABI";return false;}const auto index=static_cast<uint8_t>(i);api_.SetCoinValue(index,static_cast<uint8_t>(c.value));api_.SetCoinEnable(index,static_cast<uint8_t>(c.enabled));if(c.lockout_invert){if(!api_.SetLockoutInvert){error_="production Amber lockout configuration export is unavailable";return false;}api_.SetLockoutInvert(index,static_cast<uint8_t>(c.lockout_invert));}} if(config_.coins.route_apply_mask||config_.coins.configuration_flags){error_="production Amber cannot represent requested extended coin configuration";return false;} }
    if(config_.flags&FABRIC_AMBER_CONFIGURE_PERCENTAGE){if(!api_.SetPercent){error_="production Amber percentage configuration export is unavailable";return false;}api_.SetPercent(static_cast<uint8_t>(config_.percentage_switch));} return true;
  }
  void release_inputs(){if(started_&&!stopped_)for(uint8_t i:asserted_)api_.TurnSwitchOff(i);asserted_.clear();}
  FabricResult ok(){error_.clear();return FABRIC_OK;} FabricResult fail(const std::string&m){error_=m;return FABRIC_BACKEND_ERROR;} FabricResult invalid(const std::string&m){error_=m;return FABRIC_INVALID_ARGUMENT;} FabricResult unsupported(const std::string&m){error_=m;return FABRIC_NOT_SUPPORTED;}
  std::unique_ptr<AmberDynamicLibrary> library_; LegacyApi api_{}; std::vector<std::string> program_,sound_; FabricAmberConfigurationV1 config_{}; std::set<uint8_t> asserted_; std::string error_; uint64_t remainder_=0,sequence_=0; bool started_=false,stopped_=false; FabricResult shutdown_result_=FABRIC_OK;
};

template<typename T> bool required(AmberDynamicLibrary &lib,T &fn,const char *name,const std::string &path,std::string &error){fn=resolve<T>(lib,name);if(fn)return true;error="Failed to initialise production Amber adapter for '"+path+"': required export '"+name+"' was not found during export resolution.";return false;}
}

FabricResult CreateLegacyAmberInstance(const FabricLaunchRequest &request,std::unique_ptr<AmberDynamicLibrary> library,std::unique_ptr<FabricBackendInstance> &out,std::string &error) noexcept {
 try {
  LegacyApi a{};
#define REQ(n) if(!required(*library,a.n,#n,request.backend_path,error)) return FABRIC_NOT_SUPPORTED
  REQ(Initialise);REQ(Shutdown);REQ(Reset);REQ(Run);REQ(LoadROM);REQ(GetOutputSnapshotSize);REQ(GetOutputSnapshot);REQ(TurnSwitchOn);REQ(TurnSwitchOff);
#undef REQ
#define OPT(n) a.n=resolve<decltype(a.n)>(*library,#n)
  OPT(LoadSoundROM);OPT(GetAudioFormat);OPT(FillAudioFrames);OPT(SetOptoInvert);OPT(SetOptoStart);OPT(SetOptoEnd);OPT(SetSteps);OPT(SetCoinValue);OPT(SetCoinEnable);OPT(SetLockoutVal);OPT(SetLockoutInvert);OPT(SetPercent);
#undef OPT
  std::vector<std::pair<uint32_t,std::string>> p,s;
  if(request.rom_resource_count){for(uint32_t i=0;i<request.rom_resource_count;++i){const auto&r=request.rom_resources[i];if(r.role==FABRIC_ROM_ROLE_PROGRAM)p.emplace_back(r.slot,r.path);else if(r.role==FABRIC_ROM_ROLE_SOUND)s.emplace_back(r.slot,r.path);}}
  else for(uint32_t i=0;i<request.rom_path_count;++i)p.emplace_back(i,request.rom_paths[i]);
  auto sort=[](auto&v){std::sort(v.begin(),v.end(),[](const auto&a,const auto&b){return a.first<b.first;});};sort(p);sort(s);
  std::vector<std::string> program,sound;for(auto&v:p)program.push_back(std::move(v.second));for(auto&v:s)sound.push_back(std::move(v.second));
  const auto *config=request.machine_configuration_size?static_cast<const FabricAmberConfigurationV1*>(request.machine_configuration):nullptr;
  out=std::make_unique<LegacyInstance>(std::move(library),a,std::move(program),std::move(sound),config);return FABRIC_OK;
 } catch(const std::exception&e){error=e.what();return FABRIC_INTERNAL_ERROR;}
}
}
