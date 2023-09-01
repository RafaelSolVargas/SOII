// EPOS Network Buffer Management Utility Declarations

#ifndef __nic_buffers_h
#define __nic_buffers_h

#include <utility/debug.h>
#include <utility/list.h>
#include <utility/spin.h>
#include <system/info.h>

__BEGIN_UTIL

// Contiguous Buffer to be used to pass contiguous data directly to the DMA 
class ContiguousBuffer: private Grouping_List<char>
{
protected:
public:
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    ContiguousBuffer() {
        db<Init, NicBuffers>(TRC) << "ContiguousBuffer() => " << this << endl;
    }

    ContiguousBuffer(void * addr, unsigned long bytes) {
        db<Init, NicBuffers>(TRC) << "ContiguousBuffer(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        long * addr_long = reinterpret_cast<long *>(addr);

        // Minimum and maximum address that could be returned by alloc()
        minimum_address = addr_long;
        maximum_address = addr_long + bytes;

        internal_free(addr, bytes);
    }

    /// @brief Defines if a address is between the range of the addresses that belongs to this Buffer
    /// @param ptr address to verify 
    /// @return 
    bool contains_pointer(void * ptr) {
        long * addr = reinterpret_cast<long *>(ptr);
   
        return addr >= minimum_address && addr <= maximum_address;
    }

    /// @brief Allocate bytes contiguous size of memory in the Buffer, returning the address that was allocated
    /// @param bytes Quant of bytes to be allocated
    /// @return 
    /// @throws out_of_memory if was not found contiguous space for allocate for required bytes.
    void * alloc(unsigned long bytes) {
        db<NicBuffers>(TRC) << "ContiguousBuffer(this=" << this << ",bytes=" << bytes;

        if(!bytes)
            return 0;

        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;

        bytes += sizeof(long);        // add room for size
        if(bytes < sizeof(Element))
            bytes = sizeof(Element);

        Element * e = search_decrementing(bytes);
        if(!e) {
            out_of_memory(bytes);
            return 0;
        }

        long * addr = reinterpret_cast<long *>(e->object() + e->size());

        *addr++ = bytes;

        db<Thread>(TRC) << ") => " << reinterpret_cast<void *>(addr) << endl;

        return addr;
    }

    /// @brief Deallocate the previous allocated memory in the Buffer
    /// @param ptr The address returned by the alloc() method
    void free(void * ptr) {
        long * addr = reinterpret_cast<long *>(ptr);

        unsigned long bytes = *--addr;

        internal_free(addr, bytes);
    }


private:
    void internal_free(void * ptr, unsigned long bytes) {
        db<NicBuffers>(TRC) << "ContiguousBuffer::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if(ptr && (bytes >= sizeof(Element))) {
            Element * e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element * m1, * m2;
            insert_merging(e, &m1, &m2);
        }
    }

    static long * minimum_address;
    static long * maximum_address;

    void out_of_memory(unsigned long bytes);
};

__END_UTIL

#endif
