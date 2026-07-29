#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"

#include <cstring>
#include <iostream>
#include <string>

#define CHECK(x) do { if(!(x)){std::cerr<<"check failed line "<<__LINE__<<": " #x "\n";return 1;} } while(0)

static FabricLaunchRequest request(const char *path) {
  FabricLaunchRequest r{};r.struct_size=sizeof(r);r.struct_version=FABRIC_ABI_VERSION_1;
  std::strcpy(r.backend_kind,"amber-api-v2");std::strcpy(r.machine_identifier,"jpm-system6");std::strncpy(r.backend_path,path,sizeof(r.backend_path)-1);return r;
}
static std::string error(FabricRuntime *r){char b[512]{};uint32_t n=0;FabricRuntimeGetLastError(r,b,sizeof(b),&n);return b;}

int main(){
  FabricRuntime *runtime=nullptr;CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_1,&runtime)==FABRIC_OK);
  auto bad=request(FAKE_AMBER_NO_API_PATH);FabricMachineSession *session=nullptr;
  CHECK(FabricCreateSession(runtime,&bad,&session)==FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'Initialise'")!=std::string::npos);
  const char *program[] = {"program-even.rom","program-odd.rom"};
  FabricRomResource roms[3]{};for(auto&r:roms){r.struct_size=sizeof(r);r.struct_version=FABRIC_ABI_VERSION_1;}
  roms[0].role=FABRIC_ROM_ROLE_PROGRAM;roms[0].slot=1;roms[0].path=program[1];roms[1].role=FABRIC_ROM_ROLE_SOUND;roms[1].slot=0;roms[1].path="sound.rom";roms[2].role=FABRIC_ROM_ROLE_PROGRAM;roms[2].slot=0;roms[2].path=program[0];
  FabricAmberConfigurationV1 c{};c.magic=FABRIC_AMBER_CONFIGURATION_MAGIC;c.struct_size=sizeof(c);c.version=FABRIC_AMBER_CONFIGURATION_VERSION_1;c.flags=FABRIC_AMBER_CONFIGURE_REELS|FABRIC_AMBER_CONFIGURE_COINS|FABRIC_AMBER_CONFIGURE_PERCENTAGE;c.reels.struct_size=sizeof(c.reels);c.reels.version=AMBER_REEL_CONFIGURATION_VERSION_1;c.reels.reel_count=1;c.reels.apply_mask=1;c.reels.reels[0].steps=9;c.coins.struct_size=sizeof(c.coins);c.coins.version=AMBER_COIN_CONFIGURATION_VERSION_1;c.coins.channel_apply_mask=1;c.coins.channels[0].value=4;c.coins.channels[0].enabled=1;c.percentage_switch=7;
  auto good=request(FAKE_AMBER_LEGACY_PATH);good.rom_resources=roms;good.rom_resource_count=3;good.machine_configuration=&c;good.machine_configuration_size=sizeof(c);
  CHECK(FabricCreateSession(runtime,&good,&session)==FABRIC_OK);CHECK(FabricSessionInitialise(session)==FABRIC_OK);CHECK(FabricSessionReset(session)==FABRIC_OK);
  CHECK(FabricSessionAdvance(session,124)==FABRIC_OK);CHECK(FabricSessionAdvance(session,1)==FABRIC_OK);
  FabricInput input{};input.struct_size=sizeof(input);input.struct_version=FABRIC_ABI_VERSION_1;input.numerical_index=7;input.active=1;CHECK(FabricSessionSubmitInput(session,&input)==FABRIC_OK);
  FabricLamp lamps[2]{};FabricReel reels[1]{};FabricCharacterDisplay chars[1]{};FabricSegmentDisplay segments[1]{};FabricMachineSnapshot snap{};snap.struct_size=sizeof(snap);snap.struct_version=FABRIC_ABI_VERSION_1;snap.lamps=lamps;snap.lamp_capacity=2;snap.reels=reels;snap.reel_capacity=1;snap.character_displays=chars;snap.character_display_capacity=1;snap.segment_displays=segments;snap.segment_display_capacity=1;
  CHECK(FabricSessionGetSnapshot(session,&snap)==FABRIC_OK);CHECK(lamps[0].logical_state==1&&lamps[1].logical_state==1&&lamps[1].brightness==4.0f);CHECK(reels[0].position==10);CHECK(chars[0].characters[0]==0x1234&&chars[0].attributes[0]==1);CHECK(segments[0].segment_masks[0]==0x5a);
  FabricAudioFormat format{};format.struct_size=sizeof(format);format.struct_version=FABRIC_ABI_VERSION_1;CHECK(FabricSessionGetAudioFormat(session,&format)==FABRIC_OK);CHECK(format.sample_rate==44100&&format.channel_count==2);
  int16_t audio[8]{};uint32_t written=0;CHECK(FabricSessionReadAudio(session,audio,4,&written)==FABRIC_OK);CHECK(written==2&&audio[0]==100&&audio[3]==103);
  CHECK(FabricSessionShutdown(session)==FABRIC_OK);FabricDestroySession(session);
  /* A second load proves the first module was unloadable and its singleton reset. */
  session=nullptr;CHECK(FabricCreateSession(runtime,&good,&session)==FABRIC_OK);CHECK(FabricSessionInitialise(session)==FABRIC_OK);CHECK(FabricSessionShutdown(session)==FABRIC_OK);FabricDestroySession(session);FabricDestroyRuntime(runtime);return 0;
}
