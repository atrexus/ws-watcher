#pragma once

#include <Windows.h>

#include <memory>
#include <print>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace ws
{
    /// <summary>
    /// The watcher class that monitors the working set for any changes.
    /// </summary>
    class watcher final
    {
        static std::unique_ptr< watcher > instance;

        HANDLE handle;
        std::unique_ptr< std::jthread > thread;
        std::vector< std::uintptr_t > watch_list;

        /// <summary>
        /// Creates a new instance of the watcher class.
        /// </summary>
        /// <param name="handle">The handle to the process.</param>
        explicit watcher( HANDLE handle );

        /// <summary>
        /// Watches the working set for any changes.
        /// </summary>
        /// <param name="token">The cancellation token.</param>
        void watch( std::stop_token token ) const;

       public:
        /// <summary>
        /// Gets the global instance of the working set watcher.
        /// </summary>
        static std::unique_ptr< watcher >& get( );

        /// <summary>
        /// Adds a virtual address to the working set watch list.
        /// </summary>
        void add( const std::uintptr_t va );

        /// <summary>
        /// Stops the working set watcher.
        /// </summary>
        void stop( );
    };

    /// <summary>
    /// The paged pointer class that protects the memory from being accessed by external processes.
    /// </summary>
    template< typename T >
    class paged_ptr final
    {
        T* instance;
        mutable bool locked;

        // Note: The constructor is private to prevent the user from assigning non heap allocated objects. They must use the static constructor
        // instead.
        paged_ptr( T* instance ) : instance( instance ), locked( false )
        {
            // We initially unlock the memory so that the page is not in the working set.
            if ( instance )
                VirtualUnlock( reinterpret_cast< LPVOID >( instance ), sizeof( T ) );

            //std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

            // Add the instance to the watch list.
            watcher::get( )->add( reinterpret_cast< std::uintptr_t >( instance ) );
        }

       public:
        /// <summary>
        /// Destroys the current instance of the paged pointer.
        /// </summary>
        ~paged_ptr( )
        {
            if ( instance )
            {
                // Make sure the memory is in the working set before we free it.
                VirtualLock( reinterpret_cast< LPVOID >( instance ), sizeof( T ) );

                // Free it!
                VirtualFree( reinterpret_cast< LPVOID >( instance ), 0, MEM_RELEASE );
            }
        }

        /// <summary>
        /// Creates a new instance of the paged pointer by moving the other paged pointer.
        /// </summary>
        /// <param name="other">The other paged pointer.</param>
        paged_ptr( paged_ptr&& other ) noexcept : instance( other.instance ), locked( other.locked )
        {
            other.instance = nullptr;
            other.locked = false;
        }

        /// <summary>
        /// Assigns the current paged pointer by moving the other paged pointer.
        /// </summary>
        /// <param name="other">The other paged pointer.</param>
        paged_ptr& operator=( paged_ptr&& other ) noexcept
        {
            if ( this != &other )
            {
                if ( instance )
                    delete instance;

                instance = other.instance;
                locked = other.locked;

                other.instance = nullptr;
                other.locked = false;
            }

            return *this;
        }

        paged_ptr( const paged_ptr& ) = delete;
        paged_ptr& operator=( const paged_ptr& ) = delete;

        /// <summary>
        /// Moves the memory into the working set and returns a shared pointer to the memory. This is the only way to access the memory.
        /// </summary>
        std::shared_ptr< T > lock( ) const
        {
            // If the current memory is not locked in the working set, we lock it.
            if ( !locked && instance )
            {
                VirtualLock( reinterpret_cast< LPVOID >( instance ), sizeof( T ) );
                locked = true;
            }

            return std::shared_ptr< T >(
                instance,
                []( T* ptr )
                {
                    // Now we call virtual unlock twice to first, unlock the memory and second, remove the page from the working set.
                    VirtualUnlock( reinterpret_cast< LPVOID >( ptr ), sizeof( T ) );
                    VirtualUnlock( reinterpret_cast< LPVOID >( ptr ), sizeof( T ) );
                } );
        }

        /// <summary>
        /// Checks if the current paged pointer is valid.
        /// </summary>
        operator bool( ) const
        {
            return instance != nullptr;
        }

        template< typename T, typename... Args >
        friend paged_ptr< T > make_paged( Args&&... args );
    };

    template< typename T, typename... Args >
    paged_ptr< T > make_paged( Args&&... args )
    {
        const auto buffer = VirtualAlloc( nullptr, sizeof( T ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );

        if ( !buffer )
            throw std::bad_alloc( );

        const auto instance = new ( buffer ) T( std::forward< Args >( args )... );

        return paged_ptr< T >( instance );
    }

}  // namespace ws