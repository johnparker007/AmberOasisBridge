import pathlib, shutil, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=r'''
#include "AmberBridgeV2Adapter.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
int main() {
 uint32_t q=99; assert(amber_v2::ToQ16_16(0,q)&&q==0); assert(amber_v2::ToQ16_16(0.5,q)&&q==32768);
 assert(amber_v2::ToQ16_16(1.0/65536.0/2.0,q)&&q==1); assert(amber_v2::ToQ16_16(1e20,q)&&q==UINT32_MAX);
 assert(!amber_v2::ToQ16_16(NAN,q)); assert(!amber_v2::ToQ16_16(INFINITY,q)); assert(!amber_v2::ToQ16_16(-INFINITY,q));
 assert(amber_v2::MapAlphaPunctuation(6)==0); assert(amber_v2::MapAlphaPunctuation('.')==AMBER_ALPHA_DECIMAL_POINT);
 assert(amber_v2::MapAlphaPunctuation(',')==AMBER_ALPHA_COMMA_TAIL); assert(amber_v2::MapAlphaPunctuation('x')==0);
 uint16_t segments[16]{};uint8_t punctuation[16]{};for(uint32_t i=0;i<16;i++)segments[i]=static_cast<uint16_t>(0x8000u+i);punctuation[1]='.';punctuation[2]=',';punctuation[3]='x';AmberAlphaDisplayStateV1 alpha{};assert(amber_v2::ConvertAlpha(segments,punctuation,1.0,alpha));for(uint32_t i=0;i<16;i++)assert(alpha.segment_masks[i]==segments[i]);assert(alpha.dot_comma[0]==0&&alpha.dot_comma[1]==1&&alpha.dot_comma[2]==2&&alpha.dot_comma[3]==0&&alpha.brightness_q16_16==65536);
 uint32_t on[256]{}; double bright[256]{}; AmberSevenSegmentStateV1 displays[16]{};
 for(uint32_t d=0;d<16;d++){ on[d*16+(d%8)]=1; bright[d*16+7]=(d+1)/32.0; on[d*16+8]=1; bright[d*16+8]=1.0; }
 assert(amber_v2::ConvertSevenSegmentPlane(on,bright,displays));
 for(uint32_t d=0;d<16;d++){ assert(displays[d].segment_mask==(0x80u>>(d%8))); assert(displays[d].brightness_q16_16==(d+1)*2048); }
 uint32_t samples=0,bytes=0; assert(amber_v2::AudioExtent(UINT32_MAX/4,2,samples,bytes)); assert(bytes==UINT32_MAX-3);
 assert(!amber_v2::AudioExtent(UINT32_MAX/4+1,2,samples,bytes));
 AmberReelConfigurationV1 retainedReels{},u{}; bool hasReels=false; u.apply_mask=1;u.reels[0].steps=10;amber_v2::MergeReels(retainedReels,hasReels,u);
 u={};u.apply_mask=2;u.reels[1].steps=20;amber_v2::MergeReels(retainedReels,hasReels,u);assert(retainedReels.apply_mask==3&&retainedReels.reels[0].steps==10&&retainedReels.reels[1].steps==20);
 u={};u.apply_mask=1;u.reels[0].enabled=0;u.reels[0].steps=30;amber_v2::MergeReels(retainedReels,hasReels,u);assert(retainedReels.apply_mask==3&&retainedReels.reels[0].steps==30&&retainedReels.reels[1].steps==20);
 AmberCoinConfigurationV1 retainedCoins{},c{};bool hasCoins=false;c.channel_apply_mask=1;c.channels[0].value=1;c.configuration_flags=AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT;c.lockout_port_value=9;amber_v2::MergeCoins(retainedCoins,hasCoins,c);
 c={};c.channel_apply_mask=2;c.channels[1].value=2;c.route_apply_mask=4;c.routes[2].level=3;amber_v2::MergeCoins(retainedCoins,hasCoins,c);assert(retainedCoins.channel_apply_mask==3&&retainedCoins.route_apply_mask==4&&retainedCoins.channels[0].value==1&&retainedCoins.channels[1].value==2&&retainedCoins.configuration_flags==1&&retainedCoins.lockout_port_value==9);
 return 0;
}'''
class AdapterRuntimeTests(unittest.TestCase):
 def test_production_adapter_helpers_execute(self):
  compiler=shutil.which('c++')
  if not compiler:self.skipTest('c++ unavailable')
  with tempfile.TemporaryDirectory() as d:
   source=pathlib.Path(d)/'adapter.cpp'; binary=pathlib.Path(d)/'adapter';source.write_text(SOURCE)
   subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),'-I',str(ROOT/'src/Bridge'),str(source),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
