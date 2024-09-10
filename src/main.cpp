#include "watcher.hpp"

int main( )
{
    try
    {
        const auto& watcher = ws::watcher::get( );

        const auto& ptr = ws::make_paged< std::uint8_t >( 10 );

        std::getchar( );

        // Update the bytes
        if ( const auto& p = ptr.lock( ) )
        {
            const auto& bytes = p.get( );

            bytes[ 0 ] = 0xff;
            bytes[ 2 ] = 0xff;
        }

        std::getchar( );

        watcher->stop( );
    }
    catch ( const std::exception& e )
    {
        std::printf( "An error occurred: %s\n", e.what( ) );
    }

    return 0;
}