#include "watcher.hpp"

#include <Psapi.h>

#include <format>
#include <functional>
#include <string>

namespace ws
{
    static constexpr std::size_t page_size = 0x1000;

    static constexpr std::uintptr_t page_align( const std::uintptr_t va )
    {
        return va & ~( page_size - 1 );
    }

    static std::uint32_t get_process_id( std::uint64_t tid )
    {
        // Open a handle to the thread.
        const auto thread = OpenThread( THREAD_QUERY_INFORMATION, FALSE, static_cast< DWORD >( tid ) );

        if ( thread == nullptr )
            throw std::runtime_error( std::format( "Failed to open thread: {0}", GetLastError( ) ) );

        // Get the process ID from the thread.
        DWORD pid = GetProcessIdOfThread( thread );

        // Close the thread handle.
        CloseHandle( thread );

        return pid;
    }

    static std::wstring get_process_path( std::uint32_t pid )
    {
        // Open a handle to the process.
        const auto process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );

        if ( process == nullptr )
            throw std::runtime_error( std::format( "Failed to open process: {0}", GetLastError( ) ) );

        // Get the process path.
        std::wstring path( MAX_PATH, L'\0' );
        auto cb = static_cast< DWORD >( path.size( ) );

        if ( !QueryFullProcessImageNameW( process, 0, path.data( ), &cb ) )
        {
            CloseHandle( process );
            throw std::runtime_error( std::format( "Failed to query process image name: {0}", GetLastError( ) ) );
        }

        // Close the process handle.
        CloseHandle( process );

        return path;
    }

    std::unique_ptr< watcher > watcher::instance = nullptr;

    watcher::watcher( HANDLE handle ) : handle( handle )
    {
        // Initialize the process for working set monitoring.
        if ( !InitializeProcessForWsWatch( handle ) )
            throw std::runtime_error( std::format( "Failed to initialize process for working set watch: {0}", GetLastError( ) ) );
    }

    void watcher::watch( std::stop_token token ) const
    {
        std::vector< PSAPI_WS_WATCH_INFORMATION_EX > buffer( 100 );

        while ( !token.stop_requested( ) )
        {
            const auto size = buffer.size( );
            auto cb = static_cast< DWORD >( size * sizeof( PSAPI_WS_WATCH_INFORMATION_EX ) );

            // Clear the buffer and resize it to the required size.
            buffer.clear( );
            buffer.resize( size );

            if ( !GetWsChangesEx( handle, buffer.data( ), &cb ) )
            {
                const auto error = GetLastError( );

                // This isn't an error, just no changes in the working set since the last call.
                if ( error == ERROR_NO_MORE_ITEMS )
                {
                    // Wait a bit until we try again
                    std::this_thread::sleep_for( 1s );
                    continue;
                }

                // Any other error code is a real error.
                if ( error != ERROR_INSUFFICIENT_BUFFER )
                    throw std::runtime_error( std::format( "Failed to get working set changes: {0}", error ) );

                // Resize the buffer to the required size.
                buffer.resize( cb / sizeof( PSAPI_WS_WATCH_INFORMATION_EX ) );
                continue;
            }

            // At this point, we have an array of pages that have been added or removed from the working set. Now lets see if we have a page that we
            // care about.
            for ( const auto& entry : buffer )
            {
                if ( entry.BasicInfo.FaultingPc == nullptr )
                    continue;

                const auto faulting_va = reinterpret_cast< std::uintptr_t >( entry.BasicInfo.FaultingVa );
                const auto faulting_page_va = page_align( faulting_va );

                // Check if the current page is in the watch list.
                if ( std::find( watch_list.begin( ), watch_list.end( ), faulting_page_va ) != watch_list.end( ) )
                {
                    // Get the the process id of the thread that caused the fault.
                    const auto pid = get_process_id( entry.FaultingThreadId );

                    // If this is our process, then we can ignore it.
                    if ( pid == GetCurrentProcessId( ) )
                        continue;

                    // Print the faulting PC and the faulting VA.
                    std::println(
                        "[+] 0x{:x} (0x{:x}) was mapped by (TID: {}) @ {}",
                        faulting_page_va,
                        faulting_va,
                        entry.FaultingThreadId,
                        entry.BasicInfo.FaultingPc );

                    // Get the process path.
                    const auto path = get_process_path( pid );

                    // Print the process path.
                    std::printf( "\t--> %ws (PID: %lu)\n", path.c_str( ), pid );
                }
            }

            // Wait a bit until we try again
            std::this_thread::sleep_for( 1s );
        }
    }

    void watcher::add( const std::uintptr_t va )
    {
        std::println( "[+] Adding 0x{0:x} to the watch list.", va );

        watch_list.push_back( va );
    }

    std::unique_ptr< watcher >& watcher::get( )
    {
        if ( instance == nullptr )
        {
            instance = std::unique_ptr< watcher >( new watcher{ GetCurrentProcess( ) } );

            instance->thread.reset( new std::jthread( std::bind_front( &watcher::watch, instance.get( ) ) ) );
            instance->thread->detach( );
        }

        return instance;
    }

    void watcher::stop( )
    {
        thread->request_stop( );

        if ( thread->joinable( ) )
            thread->join( );
    }

}  // namespace ws