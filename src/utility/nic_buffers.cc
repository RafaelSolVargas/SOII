// EPOS Heap Utility Implementation

#include <utility/nic_buffers.h>

extern "C" { void _panic(); }

__BEGIN_UTIL

long * ContiguousBuffer::maximum_address = nullptr;
long * ContiguousBuffer::minimum_address = nullptr;

void ContiguousBuffer::out_of_memory(unsigned long bytes)
{
    db<Heaps, System>(ERR) << "ContiguousBuffer::alloc(this=" << this << "): out of memory while allocating " << bytes << " bytes!" << endl;

    _panic();
}

__END_UTIL
