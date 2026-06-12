#include <Windows.h>
#include <string>
#include "inline_hook.hpp"

__int32 __stdcall DllMain( void* module, unsigned __int32 reason, void* ) {
    if ( reason == DLL_PROCESS_ATTACH ) {
        DisableThreadLibraryCalls( ( HMODULE ) module );
    }
    return 1;
}