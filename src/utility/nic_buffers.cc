// EPOS Heap Utility Implementation

#include <utility/nic_buffers.h>
#include <buffers_handler.h>

extern "C" { void _panic(); }

__BEGIN_SYS

// Implemente os métodos da classe BuffersHandler
template <class T>
CBuffer * BuffersHandler<T>::Cbuffer() {
    return System::_Cbuffer;
}

template <class T>
NonCBuffer * BuffersHandler<T>::NCbuffer() {
    return System::_NCbuffer;
}

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
