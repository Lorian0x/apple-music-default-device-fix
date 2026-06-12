inline void write_jump( void* at, void* target ) {
    unsigned __int8 bytes[ 12 ] = { 72, 184 };
    *( void** ) ( bytes + 2 ) = target;
    bytes[ 10 ] = 255;
    bytes[ 11 ] = 224;

    DWORD access;
    VirtualProtect( at, sizeof( bytes ), PAGE_EXECUTE_READWRITE, &access );
    std::memcpy( at, bytes, sizeof( bytes ) );
}

inline void* install_hook( unsigned __int32 offset, void* function, void* redirection ) {
    void* caller = VirtualAlloc( nullptr, offset + 12, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
    std::memcpy( caller, function, offset );
    write_jump( ( unsigned __int8* ) caller + offset, ( unsigned __int8* ) function + offset );
    write_jump( function, redirection );
    return caller;
}