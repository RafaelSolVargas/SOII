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
    friend class Init_Application;                                              // for _heap with multiheap = false
    friend void CPU::Context::load() const volatile;
    friend void * ::malloc(size_t);						// for _heap
    friend void ::free(void *);							// for _heap
    friend void * ::operator new(size_t, const EPOS::System_Allocator &);	// for _heap
    friend void * ::operator new[](size_t, const EPOS::System_Allocator &);	// for _heap
    friend void ::operator delete(void *);					// for _heap
    friend void ::operator delete[](void *);					// for _heap

public:
    /* 
    Código que seria utilizado por novas implementações de Delete
    Ainda é necessário verificar com o professor
    static void deleteFromBuffer(void * object) {
        _buffer->free(object);
    } */
    static System_Info * const info() { assert(_si); return _si; }

private:
    static void init();

private:
    static System_Info * _si;
    static char _preheap[(Traits<System>::multiheap ? sizeof(Segment) : 0) + sizeof(Heap)];
    static Segment * _heap_segment;
    static Heap * _heap;

    static char _prebuffer[(Traits<System>::multiheap ? sizeof(Segment) : 0)];
    static Segment * _buffer_segment;
    static ContiguousBuffer * _buffer;
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

        db<MMU>(TRC) << "Free of pointer=" << ptr << " from";   
        if (System::_buffer->contains_pointer(ptr)) {
            db<MMU>(TRC) << " buffer." << endl;

            return System::_buffer->free(ptr);
        }

        db<MMU>(TRC) << " heap." << endl;
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
        return _SYS::System::_buffer->alloc(bytes);
    }
    else // SYSTEM
    {
        return _SYS::System::_heap->alloc(bytes);
    } 
}

inline void * operator new[](size_t bytes, const EPOS::System_Allocator & allocator) {
    if (allocator == EPOS::System_Allocator::CONTIGUOUS_BUFFER) 
    {
        return _SYS::System::_buffer->alloc(bytes);
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
