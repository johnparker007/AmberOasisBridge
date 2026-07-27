#include "amber/amber_api.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>

static void Error(const AmberApiV1& api, AmberHandle handle, AmberResult result) {
    char text[512] = {}; uint32_t required = 0;
    api.GetLastError(handle, text, sizeof(text), &required);
    std::fprintf(stderr, "failed (%u): %s\n", static_cast<unsigned>(result), text);
}

int main(int argc, char** argv) {
    HMODULE bridge=LoadLibraryExW(L"AmberBridge.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!bridge) { std::fputs("FAIL: AmberBridge.dll could not be loaded\n",stderr); return 1; }
    auto get_api=reinterpret_cast<decltype(&AmberGetApi)>(GetProcAddress(bridge,"AmberGetApi"));
    AmberApiV1 api={}; AmberResult result;
    if (!get_api || (result=get_api(AMBER_API_VERSION_1,sizeof(api),&api))!=AMBER_OK) { std::fputs("FAIL: API negotiation\n",stderr); FreeLibrary(bridge); return 1; }
    AmberBridgeInfo bridge_info={sizeof(bridge_info)}; AmberCoreInfo core={sizeof(core)};
    api.GetBridgeInfo(&bridge_info); api.EnumerateCore(0,&core);
    std::printf("%s %s; API 0x%08x\ncore[0]: %s (%s)\n",bridge_info.name,bridge_info.bridge_version,bridge_info.api_version,core.core_id,core.display_name);
    AmberHandle handle=nullptr;
    if ((result=api.Create(core.core_id,&handle))!=AMBER_OK) { Error(api,nullptr,result); FreeLibrary(bridge); return 1; }
    std::puts("JPM DLL loaded; required exports resolved");
    if (argc>1) {
        AmberInitialiseParams params={sizeof(params)};
        for (int i=1;i<argc && i<=4;++i) params.program_roms[i-1]=argv[i];
        if ((result=api.Initialise(handle,&params))!=AMBER_OK) { Error(api,handle,result); api.Destroy(handle); FreeLibrary(bridge); return 1; }
        int32_t ran=0; uint32_t steps=100;
        if (const char* value=std::getenv("AMBER_DIAGNOSTIC_STEPS")) steps=static_cast<uint32_t>(std::strtoul(value,nullptr,10));
        if ((result=api.Run(handle,steps,&ran))!=AMBER_OK || (result=api.Shutdown(handle))!=AMBER_OK) { Error(api,handle,result); return 1; }
        std::printf("runtime lifecycle succeeded (%d cycles reported)\n",ran);
    }
    if ((result=api.Destroy(handle))!=AMBER_OK) { Error(api,handle,result); FreeLibrary(bridge); return 1; }
    FreeLibrary(bridge); std::puts("destroyed and unloaded successfully"); return 0;
}
