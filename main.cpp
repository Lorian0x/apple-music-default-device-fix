#include <Windows.h>
#include <string>
#include "inline_hook.hpp"
#include <mmdeviceapi.h>
#include <mutex>

std::wstring get_device_id( IMMDevice* device ) {
    wchar_t* id;
    device->GetId( &id );
    std::wstring result = id;
    CoTaskMemFree( id );
    return result;
}

void* refresh_engine;
void* tracked_engine;
std::mutex engine_mutex;

void reconcile_engine( IMMDeviceEnumerator* enumerator ) {
    IMMDevice* fresh;
    enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, &fresh );
    std::wstring default_id = get_device_id( fresh );

    std::lock_guard< std::mutex > lock( engine_mutex );
    if ( tracked_engine != nullptr ) {
        IMMDevice** device = ( IMMDevice** ) ( ( unsigned __int8* ) tracked_engine + 16 );
        if ( get_device_id( *device ) != default_id ) {
            fresh->AddRef( );
            ( ( IMMDevice* ) InterlockedExchangePointer( ( void** ) device, fresh ) )->Release( );
            ( ( __int64 ( * )( void* ) ) refresh_engine )( tracked_engine );
        }
    }
    fresh->Release( );
}

unsigned long __stdcall nc_ref( void* ) {
    return 1;
}

void* change_event;
long __stdcall nc_default_changed( void*, EDataFlow flow, ERole role, const wchar_t* ) {
    if ( ( flow == eRender ) * ( role == eMultimedia ) != 0 ) SetEvent( change_event );
    return S_OK;
}

void* notification_vtable[] = { ( void* ) nc_ref,( void* ) nc_ref,( void* ) nc_ref,( void* ) nc_ref,
                                ( void* ) nc_ref,( void* ) nc_ref,( void* ) nc_default_changed,( void* ) nc_ref };
void* notification_client = notification_vtable;

DWORD __stdcall worker_thread( void* ) {
    CoInitializeEx( nullptr, COINIT_MULTITHREADED );

    IMMDeviceEnumerator* enumerator;
    CoCreateInstance( __uuidof( MMDeviceEnumerator ), nullptr, CLSCTX_ALL, __uuidof( IMMDeviceEnumerator ),( void** ) &enumerator );
    enumerator->RegisterEndpointNotificationCallback( ( IMMNotificationClient* ) &notification_client );

    for ( ;; ) {
        WaitForSingleObject( change_event, INFINITE );
        reconcile_engine( enumerator );
    }
}

void* original_prepare_caller;
void redirected_prepare( void* engine ) {
    std::unique_lock< std::mutex > lock( engine_mutex, std::try_to_lock );
    if ( lock.owns_lock( ) != 0 ) tracked_engine = engine;
    ( ( void ( * )( void* ) ) original_prepare_caller )( engine );
}

void* original_close_caller;
void redirected_close( void* engine ) {
    std::lock_guard< std::mutex > lock( engine_mutex );
    if ( tracked_engine == engine ) tracked_engine = nullptr;
    ( ( void ( * )( void* ) ) original_close_caller )( engine );
}

__int32 __stdcall DllMain( void* module, unsigned __int32 reason, void* ) {
    if ( reason == DLL_PROCESS_ATTACH ) {
        DisableThreadLibraryCalls( ( HMODULE ) module );
        change_event = CreateEventW( nullptr, FALSE, FALSE, nullptr );

        unsigned __int8* base = ( unsigned __int8* ) GetModuleHandleW( L"CoreAudioToolbox.dll" );
        refresh_engine = base + 6241920;
        original_prepare_caller = install_hook( 14, base + 6237472, ( void* ) redirected_prepare );
        original_close_caller = install_hook( 15, base + 6240960, ( void* ) redirected_close );
        
        CreateThread( nullptr, 0, worker_thread, nullptr, 0, nullptr );
    }
    return 1;
}