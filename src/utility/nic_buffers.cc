// EPOS Heap Utility Implementation

#include <utility/nic_buffers.h>

extern "C" { void _panic(); }

__BEGIN_UTIL

long * CBuffer::maximum_address = nullptr;
long * CBuffer::minimum_address = nullptr;

long * NonCBuffer::maximum_address = nullptr;
long * NonCBuffer::minimum_address = nullptr;


void CBuffer::out_of_memory(unsigned long bytes)
{
    db<Heaps, System>(ERR) << "CBuffer::alloc(this=" << this << "): out of memory while allocating " << bytes << " bytes!" << endl;

    _panic();
}


void NonCBuffer::out_of_memory(unsigned long bytes)
{
    db<Heaps, System>(ERR) << "NCBuffer::alloc(this=" << this << "): out of memory while allocating " << bytes << " bytes!" << endl;

    _panic();
}

__END_UTIL
