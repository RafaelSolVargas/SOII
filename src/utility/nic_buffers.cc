// EPOS Heap Utility Implementation

#include <utility/nic_buffers.h>
#include <buffers_handler.h>

extern "C" { void _panic(); }

__BEGIN_SYS


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

__END_SYS
