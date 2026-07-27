#include "amber/amber_api.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>
#include <string>
#include <vector>

static bool ModulePath(HMODULE module, std::wstring& path) {
    for (DWORD capacity=256; capacity<=32768; capacity*=2) {
        std::vector<wchar_t> buffer(capacity); SetLastError(ERROR_SUCCESS);
        DWORD count=GetModuleFileNameW(module,buffer.data(),capacity);
        if (!count) return false;
        if (count<capacity-1 || (count<capacity && GetLastError()!=ERROR_INSUFFICIENT_BUFFER)) { path.assign(buffer.data(),count); return true; }
    }
    SetLastError(ERROR_FILENAME_EXCED_RANGE); return false;
}

static std::wstring SystemMessage(DWORD code) {
    wchar_t* text=nullptr;
    DWORD count=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr,code,0,reinterpret_cast<wchar_t*>(&text),0,nullptr);
    std::wstring result=count&&text?std::wstring(text,count):L"No system message available.";
    if(text)LocalFree(text); while(!result.empty()&&(result.back()==L'\r'||result.back()==L'\n'))result.pop_back(); return result;
}

static void PrintApiError(const AmberApiV1& api,AmberHandle handle,const char* operation,AmberResult result) {
    uint32_t required=0; api.GetLastError(handle,nullptr,0,&required);
    std::vector<char> text(required?required:1); api.GetLastError(handle,text.data(),static_cast<uint32_t>(text.size()),nullptr);
    std::fprintf(stderr,"%s failed (%u): %s\n",operation,static_cast<unsigned>(result),text.data());
}

static bool ParseSteps(uint32_t& steps) {
    char* value=nullptr; size_t length=0;
    errno_t environment_error=_dupenv_s(&value,&length,"AMBER_DIAGNOSTIC_STEPS");
    if(environment_error!=0){std::fprintf(stderr,"Could not read AMBER_DIAGNOSTIC_STEPS (errno %d)\n",environment_error);return false;}
    if(!value)return true;
    if(!*value){std::free(value);std::fputs("AMBER_DIAGNOSTIC_STEPS must not be empty\n",stderr);return false;}
    char* end=nullptr; errno=0; unsigned long long parsed=std::strtoull(value,&end,10);
    bool valid=errno!=ERANGE&&*end=='\0'&&parsed!=0&&parsed<=std::numeric_limits<uint32_t>::max();
    std::free(value);
    if(!valid) {
        std::fputs("AMBER_DIAGNOSTIC_STEPS must be an integer from 1 through 4294967295\n",stderr); return false;
    }
    steps=static_cast<uint32_t>(parsed); return true;
}

int wmain(int argc,wchar_t** wide_argv) {
    std::wstring executable;
    if(!ModulePath(nullptr,executable)){DWORD e=GetLastError();std::fwprintf(stderr,L"GetModuleFileNameW failed. Windows error %lu: %ls\n",e,SystemMessage(e).c_str());return 1;}
    size_t slash=executable.find_last_of(L"\\/");
    std::wstring directory=slash==std::wstring::npos?L"":executable.substr(0,slash+1);
    std::wstring bridge_path=directory+L"AmberBridge.dll";
    std::wstring core_path=directory+L"AmberOasis.JPMSystem6.dll";
    std::wprintf(L"AmberBridge.dll: %ls\n",bridge_path.c_str());
    HMODULE bridge=LoadLibraryExW(bridge_path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!bridge){DWORD e=GetLastError();std::fwprintf(stderr,L"Failed to load bridge:\n%ls\nWindows error %lu: %ls\n",bridge_path.c_str(),e,SystemMessage(e).c_str());return 1;}
    int exit_code=1; AmberHandle handle=nullptr; bool initialised=false; AmberApiV1 api{};
    auto get_api=reinterpret_cast<decltype(&AmberGetApi)>(GetProcAddress(bridge,"AmberGetApi"));
    if(!get_api){DWORD e=GetLastError();std::fprintf(stderr,"AmberGetApi resolution failed. Windows error %lu\n",e);goto cleanup;}
    {
        alignas(AmberApiV1) unsigned char oversized[sizeof(AmberApiV1)+16]; std::memset(oversized,0xA5,sizeof(oversized));
        AmberApiV1 probe{};
        if(get_api(AMBER_API_VERSION_1,sizeof(probe),nullptr)!=AMBER_INVALID_ARGUMENT ||
           get_api(AMBER_API_VERSION_1-1,sizeof(probe),&probe)!=AMBER_UNSUPPORTED_VERSION ||
           get_api(AMBER_API_VERSION_1,sizeof(probe)-1,&probe)!=AMBER_BUFFER_TOO_SMALL ||
           get_api(AMBER_API_VERSION_1,sizeof(oversized),reinterpret_cast<AmberApiV1*>(oversized))!=AMBER_OK) {
            std::fputs("API negotiation contract probes failed\n",stderr); goto cleanup;
        }
        for(size_t i=sizeof(AmberApiV1);i<sizeof(oversized);++i)if(oversized[i]!=0xA5){std::fputs("API negotiation overwrote the caller tail\n",stderr);goto cleanup;}
        AmberResult result=get_api(AMBER_API_VERSION_1,sizeof(api),&api);
        if(result!=AMBER_OK){std::fprintf(stderr,"API negotiation failed (%u)\n",static_cast<unsigned>(result));goto cleanup;}
        AmberBridgeInfo info{sizeof(info)}; AmberCoreInfo core{sizeof(core)};
        if((result=api.GetBridgeInfo(&info))!=AMBER_OK){PrintApiError(api,nullptr,"GetBridgeInfo",result);goto cleanup;}
        std::printf("Amber Bridge version: %s\nAPI version: 0x%08x\n",info.bridge_version,info.api_version);
        if((result=api.EnumerateCore(0,&core))!=AMBER_OK){PrintApiError(api,nullptr,"EnumerateCore",result);goto cleanup;}
        std::printf("Core: %s (%s)\n",core.core_id,core.display_name);
        if((result=api.Create(core.core_id,&handle))!=AMBER_OK){PrintApiError(api,nullptr,"Create",result);goto cleanup;}
        { AmberHandle second=nullptr; if(api.Create(core.core_id,&second)!=AMBER_INSTANCE_LIMIT||second!=nullptr){std::fputs("One-instance policy probe failed\n",stderr);goto cleanup;} }
        std::wprintf(L"JPM DLL: %ls\n",core_path.c_str()); std::puts("Required JPM exports: resolved successfully");
        if(argc>1) {
            uint32_t steps=100; if(!ParseSteps(steps))goto cleanup;
            std::vector<std::string> args; args.reserve(static_cast<size_t>(argc-1));
            for(int i=1;i<argc&&i<=4;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);if(n<=0){std::fputs("ROM path UTF-8 conversion failed\n",stderr);goto cleanup;}std::string s(static_cast<size_t>(n),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,s.data(),n,nullptr,nullptr);s.pop_back();args.push_back(s);}
            AmberInitialiseParams params{sizeof(params)}; for(size_t i=0;i<args.size();++i)params.program_roms[i]=args[i].c_str();
            result=api.Initialise(handle,&params); std::printf("Initialise result: %u\n",static_cast<unsigned>(result));
            if(result!=AMBER_OK){PrintApiError(api,handle,"Initialise",result);goto cleanup;} initialised=true;
            int32_t ran=0; std::printf("Requested Run value: %u\n",steps); result=api.Run(handle,steps,&ran);
            std::printf("Run result: %u; core-reported result: %d\n",static_cast<unsigned>(result),ran);
            if(result!=AMBER_OK){PrintApiError(api,handle,"Run",result);goto cleanup;}
            result=api.Shutdown(handle); std::printf("Shutdown result: %u\n",static_cast<unsigned>(result));
            if(result!=AMBER_OK){PrintApiError(api,handle,"Shutdown",result);goto cleanup;} initialised=false;
        }
        AmberHandle stale=handle; result=api.Destroy(handle); std::printf("Destroy result: %u\n",static_cast<unsigned>(result));
        if(result!=AMBER_OK){PrintApiError(api,handle,"Destroy",result);goto cleanup;} handle=nullptr;
        if(api.Reset(stale)!=AMBER_INVALID_STATE){std::fputs("Stale-handle safety probe failed\n",stderr);goto cleanup;}
        std::puts("Stale-handle safety: success"); exit_code=0;
    }
cleanup:
    if(handle&&api.struct_size){if(initialised){AmberResult r=api.Shutdown(handle);std::printf("Cleanup Shutdown result: %u\n",static_cast<unsigned>(r));}AmberResult r=api.Destroy(handle);std::printf("Cleanup Destroy result: %u\n",static_cast<unsigned>(r));}
    if(!FreeLibrary(bridge)){DWORD e=GetLastError();std::fprintf(stderr,"Bridge unload failed. Windows error %lu\n",e);return 1;}
    std::puts("AmberBridge.dll unload: success"); return exit_code;
}
