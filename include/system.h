// EPOS System/Application Containers and Dynamic Memory Allocators Declarations

#ifndef __system_h
#define __system_h

#include <utility/nic_buffers.h>
#include <utility/string.h>
#include <utility/heap.h>
#include <system/info.h>
#include <memory.h>

__BEGIN_SYS

class Application
{
    friend class Init_Application;
    friend void * ::malloc(size_t);
    friend void ::free(void *);

private:
    static void init();

private:
    static char _preheap[sizeof(Heap)];
    static Heap * _heap;
};

class System
{
    friend class Init_System;                                                   // for _heap and _buffer
    friend class Init_Application;   
    template<class>                                           // for _heap with multiheap = false
    friend class BuffersHandler;                            // for BufferHandler access the Buffers
    friend void CPU::Context::load() const volatile;
    friend void * ::malloc(size_t);						// for _heap
    friend void ::free(void *);							// for _heap
    friend void * ::operator new(size_t, const EPOS::System_Allocator &);	// for _heap
    friend void * ::operator new[](size_t, const EPOS::System_Allocator &);	// for _heap
    friend void ::operator delete(void *);					// for _heap
    friend void ::operator delete[](void *);					// for _heap

public:
    static System_Info * const info() { assert(_si); return _si; }

private:
    static void init();

private:
    static System_Info * _si;
    static char _preheap[(Traits<System>::multiheap ? sizeof(Segment) : 0) + sizeof(Heap)];
    static Segment * _heap_segment;
    static Heap * _heap;

    static char _preCbuffer[(Traits<System>::buffer_enable ? sizeof(Segment) : 0)];
    static Segment * _Cbuffer_segment;
    static CBuffer * _Cbuffer;

    static char _preNCbuffer[(Traits<System>::buffer_enable ? sizeof(Segment) : 0)];
    static Segment * _NCbuffer_segment;
    static NonCBuffer * _NCbuffer;

    static SiFiveU_NIC * _nic;
};

__END_SYS

extern "C"
{
    // Standard C Library allocators
    inline void * malloc(size_t bytes) {
        __USING_SYS;
        if(Traits<System>::multiheap)
            return Application::_heap->alloc(bytes);
        else
            return System::_heap->alloc(bytes);
    }

    inline void * calloc(size_t n, unsigned int bytes) {
        void * ptr = malloc(n * bytes);
        memset(ptr, 0, n * bytes);
        return ptr;
    }

    inline void free(void * ptr) {
        __USING_SYS;

        if (System::_Cbuffer->contains_pointer(ptr)) {
            db<MMU>(TRC) << "Pointer=" << ptr << " deleted from CBuffer" << endl;;   

            return System::_Cbuffer->free(ptr);
        }

        if (System::_NCbuffer->contains_pointer(ptr)) {
            db<MMU>(TRC) << "Pointer=" << ptr << " deleted from NCBuffer" << endl;;   

            return System::_NCbuffer->free(ptr);
        }

        if(Traits<System>::multiheap) 
            Heap::typed_free(ptr);
        else 
            Heap::untyped_free(System::_heap, ptr);
    }
}

// C++ dynamic memory allocators and deallocators
inline void * operator new(size_t bytes) {
    return malloc(bytes);
}

inline void * operator new[](size_t bytes) {
    return malloc(bytes);
}

inline void * operator new(size_t bytes, const EPOS::System_Allocator & allocator) {
    if (allocator == EPOS::System_Allocator::CONTIGUOUS_BUFFER) 
    {
        return _SYS::System::_Cbuffer->alloc(bytes);
    }
    else if (allocator == EPOS::System_Allocator::NON_CONTIGUOUS_BUFFER) 
    {
        return _SYS::System::_NCbuffer->alloc(bytes);
    }
    else // SYSTEM
    {
        return _SYS::System::_heap->alloc(bytes);
    } 
}

inline void * operator new[](size_t bytes, const EPOS::System_Allocator & allocator) {
    if (allocator == EPOS::System_Allocator::CONTIGUOUS_BUFFER) 
    {
        return _SYS::System::_Cbuffer->alloc(bytes);
    }
    else if (allocator == EPOS::System_Allocator::NON_CONTIGUOUS_BUFFER) 
    {
        return _SYS::System::_NCbuffer->alloc(bytes);
    }
    else // SYSTEM
    {
        return _SYS::System::_heap->alloc(bytes);
    } 
}

// Delete cannot be declared inline due to virtual destructors
void operator delete(void * ptr);
void operator delete[](void * ptr);
void operator delete(void * ptr, size_t bytes);
void operator delete[](void * ptr, size_t bytes);

#endif
