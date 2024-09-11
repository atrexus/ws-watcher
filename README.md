# ws-watcher
This application protects its heap-allocated memory from being accessed externally and internally. The working set watcher introduces a custom smart pointer, `ws::paged_ptr`, which pages the memory associated with the pointer out of the working set. The watcher then launches a separate thread that queries the working set and catches any page faults that occur. If a page fault was caused externally, information about the handle used is logged.

To safely access the memory in a paged pointer, use the `lock` method to retrieve a shared pointer instance to that data. Refer to the example below:
```cpp
// Allocate a 10 byte array of paged memory
const auto& paged = ws::make_paged< std::uint8_t >( 10 );

// Lock the page in memory so that we can safely access it's data
if ( const auto& data = paged.lock( ) )
{
    // Get the raw pointer to the data.
    const auto& ptr = data.get( );

    // Edit the memory
    ptr[ 0 ] = 0xFF;
    ptr[ 1 ] = 0xFF;
}
```

The video below demonstrates what happens if an external process attempts to read from a buffer protected by the `ws::paged_ptr` class. 

https://github.com/user-attachments/assets/e84d6526-4c43-42de-84d7-035e6690b041
