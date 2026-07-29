#include "amber/amber_api.h"
#include <algorithm>
#include <cstring>
#include <new>
struct AmberInstance_t { bool initialised=false; };
namespace {
AmberResult AMBER_CALL bridge(AmberBridgeInfo*i){if(!i)return AMBER_INVALID_ARGUMENT;i->api_version=2;i->name="Fake Amber";i->bridge_version="test";return AMBER_OK;}
AmberResult AMBER_CALL enumerate(uint32_t i,AmberCoreInfo*c){if(!c)return AMBER_INVALID_ARGUMENT;if(i)return AMBER_NO_MORE_ITEMS;c->core_id="jpm-system6";c->display_name="Fake System 6";return AMBER_OK;}
AmberResult AMBER_CALL create(const char*id,AmberHandle*h){if(!id||!h||std::strcmp(id,"jpm-system6"))return AMBER_INVALID_ARGUMENT;*h=new(std::nothrow)AmberInstance_t;return *h?AMBER_OK:AMBER_INTERNAL_ERROR;}
AmberResult AMBER_CALL destroy(AmberHandle h){delete h;return AMBER_OK;}
AmberResult AMBER_CALL initialise(AmberHandle h,const AmberInitialiseParams*){if(!h)return AMBER_INVALID_ARGUMENT;h->initialised=true;return AMBER_OK;}
AmberResult AMBER_CALL okay(AmberHandle h){return h?AMBER_OK:AMBER_INVALID_ARGUMENT;}
AmberResult AMBER_CALL run(AmberHandle h,uint32_t n,int32_t*r){if(!h||!r)return AMBER_INVALID_ARGUMENT;*r=static_cast<int32_t>(n);return AMBER_OK;}
AmberResult AMBER_CALL error(AmberHandle,char*b,uint32_t n,uint32_t*required){if(!required)return AMBER_INVALID_ARGUMENT;*required=1;if(!b||!n)return AMBER_BUFFER_TOO_SMALL;b[0]=0;return AMBER_OK;}
AmberResult AMBER_CALL caps(AmberHandle h,AmberCapabilitiesV1*c){if(!h||!c)return AMBER_INVALID_ARGUMENT;c->feature_bits=AMBER_CAP_SWITCH_INPUT|AMBER_CAP_OUTPUT_SNAPSHOT|AMBER_CAP_AUDIO;c->max_switches=256;return AMBER_OK;}
AmberResult AMBER_CALL sw(AmberHandle h,uint32_t i,uint32_t){return h&&i<256?AMBER_OK:AMBER_INVALID_RANGE;}
AmberResult AMBER_CALL snapshot(AmberHandle h,AmberOutputSnapshotV1*s){if(!h||!s)return AMBER_INVALID_ARGUMENT;s->matrix_lamp_count=1;s->matrix_lamps[0]={1,32768};s->reel_count=1;s->reel_positions[0]=-3;s->alpha_display_count=1;s->alpha_displays[0].segment_masks[0]=0x1234;s->alpha_displays[0].dot_comma[0]=AMBER_ALPHA_DECIMAL_POINT;s->seven_segment_display_count=1;s->seven_segment_displays[0].segment_mask=0xabcdef;return AMBER_OK;}
AmberResult AMBER_CALL format(AmberHandle h,AmberAudioFormatV1*f){if(!h||!f)return AMBER_INVALID_ARGUMENT;f->sample_rate=44100;f->channels=2;f->sample_format=AMBER_AUDIO_SAMPLE_PCM_S16;f->interleaving=AMBER_AUDIO_INTERLEAVED;return AMBER_OK;}
AmberResult AMBER_CALL audio(AmberHandle h,int16_t*p,uint32_t n,uint32_t*w){if(!h||!w||(n&&!p))return AMBER_INVALID_ARGUMENT;*w=std::min(n,2u);for(uint32_t i=0;i<*w*2;i++)p[i]=static_cast<int16_t>(i);return AMBER_OK;}
AmberResult AMBER_CALL reels(AmberHandle h,const AmberReelConfigurationV1*){return okay(h);} AmberResult AMBER_CALL coins(AmberHandle h,const AmberCoinConfigurationV1*){return okay(h);} AmberResult AMBER_CALL percent(AmberHandle h,uint32_t){return okay(h);}
}
extern "C" AMBER_EXPORT AmberResult AMBER_CALL AmberGetApi(uint32_t version,uint32_t size,void*out){if(version!=2)return AMBER_UNSUPPORTED_VERSION;if(!out||size<sizeof(AmberApiV2))return AMBER_INVALID_ARGUMENT;AmberApiV2 a{};a.struct_size=sizeof(a);a.api_version=2;a.GetBridgeInfo=bridge;a.EnumerateCore=enumerate;a.Create=create;a.Destroy=destroy;a.Initialise=initialise;a.Reset=okay;a.Run=run;a.Shutdown=okay;a.GetLastError=error;a.GetCapabilities=caps;a.SetSwitchState=sw;a.GetOutputSnapshot=snapshot;a.GetAudioFormat=format;a.FillAudioFrames=audio;a.ConfigureReels=reels;a.ConfigureCoins=coins;a.SetPercentageSwitch=percent;std::memcpy(out,&a,sizeof(a));return AMBER_OK;}
