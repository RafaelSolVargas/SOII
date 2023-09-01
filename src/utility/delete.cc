// EPOS Dynamic Memory Allocators Implementation

#include <system.h>

// C++ dynamic memory deallocators
void operator delete(void * object) {
    return free(object);
}

void operator delete[](void * object) {
    return free(object);
}

void operator delete(void * object, size_t bytes) {
    return free(object);
}

void operator delete[](void * object, size_t bytes) {
    return free(object);
}

/* 
Functions to allow users to define witch delete will be used, could be used to allows users
to define from where the data will be deleted like they define where it was allocated

void operator delete(void * object, size_t bytes, const EPOS::System_Allocator & allocator) {
    if (allocator == EPOS::System_Allocator::CONTIGUOUS_BUFFER) { 
        // Currently using a public method because couldn't mark as friend method
        return _SYS::System::deleteFromBuffer(object);
    }

    return free(object);
}

void operator delete[](void * object, size_t bytes, const EPOS::System_Allocator & allocator) {
    if (allocator == EPOS::System_Allocator::CONTIGUOUS_BUFFER) { 
        // Currently using a public method because couldn't mark as friend method
        return _SYS::System::deleteFromBuffer(object);
    }

    return free(object);
} */