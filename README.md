A simple memory linear allocator.

It works on Windows only. It asks for the operating system for some memory
and manage malloc and free operations. The user must init and end the allocation
calling `allocate_init` and `allocate_end`.

It aims for simplicity.

The alignment defined is 16 bytes. That means the allocator will return pointers
that are multiple of this value. 

The memory block splits after the allocation is performed, and it coalesces when 
the user frees some block, to avoid excessive fragmentation.
