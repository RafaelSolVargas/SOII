// EPOS Network Buffer Management Utility Declarations

#ifndef __nic_buffers_h
#define __nic_buffers_h

#include <utility/debug.h>
#include <utility/list.h>
#include <utility/spin.h>
#include <system/info.h>

__BEGIN_UTIL

// Contiguous Buffer to be used to pass contiguous data directly to the DMA 
class CBuffer: private Grouping_List<char>
{
protected:
public:
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    CBuffer() {
        db<Init, NicBuffers>(TRC) << "CBuffer() => " << this << endl;
    }

    CBuffer(void * addr, unsigned long bytes) {
        db<Init, NicBuffers>(TRC) << "CBuffer(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

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
        db<NicBuffers>(TRC) << "CBuffer(this=" << this << ",bytes=" << bytes;

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

        db<NicBuffers>(TRC) << ") => " << reinterpret_cast<void *>(addr) << endl;

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
        db<NicBuffers>(TRC) << "CBuffer::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

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

// Class to describe the non contiguous allocation that happened in the NonCBuffer
class MemAllocationMap
{
public:
    MemAllocationMap(unsigned long totalSize, int totalChunks)
        : _original_size(totalSize), _total_size(totalSize), _quant_chunks(totalChunks)
    {
        _chunks = new void*[totalChunks];
        _chunks_sizes = new int[totalChunks];

        for (int i = 0; i < totalChunks; i++) {
            _chunks_sizes[i] = 1;
        }
    }

    ~MemAllocationMap()
    {
        delete[] _chunks;
        delete[] _chunks_sizes;
    }

    void log_allocation() 
    {
        db<Init, NicBuffers>(TRC) << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl; 

        db<Init, NicBuffers>(TRC) << "MemAllocationMap(" << this 
                                    << ") chunks_quant=" << _quant_chunks
                                    << endl; 

        for (int i = 0; i < _quant_chunks; i++) 
        {
            db<Init, NicBuffers>(TRC) << "Chunk[" << i << "] => " 
                                        << "Address=" << _chunks[i]
                                        << ", Size=" << _chunks_sizes[i]
                                        << endl; 
        }
        db<Init, NicBuffers>(TRC) << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl; 
    }
    

    unsigned long original_size() { return _original_size; }
    unsigned long total_size() { return _total_size; }
    int quant_chunks() { return _quant_chunks; }
    void** chunks() { return _chunks; }
    int* chunks_sizes() { return _chunks_sizes; }
private:
    unsigned long _original_size;
    unsigned long _total_size;
    int _quant_chunks;
    void **_chunks;     
    int *_chunks_sizes;
};

// Contiguous Buffer to be used to pass contiguous data directly to the DMA 
class NonCBuffer: private Grouping_List<char>
{
protected:
public:
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    NonCBuffer() {
        db<Init, NicBuffers>(TRC) << "NonCBuffer() => " << this << endl;
    }

    NonCBuffer(void * addr, unsigned long bytes) {
        db<Init, NicBuffers>(TRC) << "NonCBuffer(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        long * addr_long = reinterpret_cast<long *>(addr);

        // Minimum and maximum address that could be returned by alloc()
        minimum_address = addr_long;
        maximum_address = addr_long + bytes;

        internal_free(addr, bytes);
    }

    typedef MemAllocationMap AllocationMap;

    /// @brief Defines if a address is between the range of the addresses that belongs to this Buffer
    /// @param ptr address to verify 
    /// @return 
    bool contains_pointer(void * ptr) {
        long * addr = reinterpret_cast<long *>(ptr);
   
        return addr >= minimum_address && addr <= maximum_address;
    }

    /// @brief Allocate bytes, that could be non contiguous, memory in the Buffer
    /// @param bytes Quant of bytes to be allocated
    /// @return (AllocationMap *) -> Pointer to the Map of the allocated memory
    /// @throws out_of_memory if was not found space for allocate for required bytes.
    AllocationMap * alloc(unsigned long bytes) {
        db<NicBuffers>(TRC) << "****************************"  << endl;
        db<NicBuffers>(TRC) << "NonCBuffer::alloc(bytes= " << bytes << ")" << endl;
        if(!bytes)
            return 0;

        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;

        // Verifica se eu coloco isso aqui ou dentro de cada bloco
        bytes += sizeof(long);        // add room for size
        if(bytes < sizeof(Element))
            bytes = sizeof(Element);


        // Verify if the Buffer contains space for bytes.
        if (grouped_size() < bytes) 
        {
            out_of_memory(bytes);
        }

        // Creates the MemoryMap 
        unsigned long size = bytes;
        unsigned long allocated_bytes = 0;
        unsigned long sizes_found[MAX_FRAGMENTATION];
        
        Element * elements[MAX_FRAGMENTATION];
        int elements_quant = 0;

        // While doesn't allocated all bytes or the allocation reached too many fragments
        // and the size still above the minium fragmentation size
        while (allocated_bytes < bytes && elements_quant < MAX_FRAGMENTATION && size > 8) 
        {
            db<NicBuffers>(TRC) << "NonCBuffer_Allocation >> Searching for " << bytes << " bytes" << endl;

            // Tries to find enough space for the current size
            Element * e = search_decrementing(size);
            if(!e) {
                // If not find, decrement the contiguous size searched
                size /= 2;

                // Doesn't allow unaligned size
                if(!Traits<CPU>::unaligned_memory_access)
                    while((size % sizeof(void *)))
                        ++size;


                db<NicBuffers>(TRC) << "NonCBuffer_Allocation >> Not found, decreasing size to " << size << " bytes" << endl;

                continue;
            }

            // Update the bytes already allocated
            allocated_bytes += size;
            
            // Store the allocated element and update counter
            elements[elements_quant] = e;
            sizes_found[elements_quant] = size;
            elements_quant++;

            db<NicBuffers>(TRC) << "NonCBuffer_Allocation >> " << bytes << " found. Missing " 
                                << bytes - allocated_bytes << " bytes."  << endl;
        }

        // If the required bytes was not allocated, free all elements and throw error
        if (allocated_bytes < bytes) 
        {
            for (int i = 0; i < elements_quant; i++) 
            {
                Element * e = elements[i];

                long * e_addr = reinterpret_cast<long *>(e->object() + e->size());
                
                e_addr++;
                
                free(e_addr);
            }

            out_of_memory(bytes);
        }

        // Create the map and describe the elements allocated
        AllocationMap * map = new AllocationMap(bytes, elements_quant);
        for (int i = 0; i < elements_quant; i++) 
        {
            Element * e = elements[i];

            long * e_addr = reinterpret_cast<long *>(e->object() + e->size());

            *e_addr++ = sizes_found[i];
            
            map->chunks()[i] = e_addr;

            map->chunks_sizes()[i] = sizes_found[i];
        }

        db<NicBuffers>(TRC) << "****************************"  << endl;

        return map;
    }

    /// @brief Deallocate the previous allocated memory in the Buffer
    /// @param ptr The address returned by the alloc() method
    void free(void * ptr) {
        AllocationMap * map = reinterpret_cast<AllocationMap *>(ptr);

        db<NicBuffers>(TRC) << "Deallocating map " << ptr << endl;

        map->log_allocation();

        // Iterates for all chunks allocated in the map
        for (int i = 0; i < map->quant_chunks(); i++) 
        {
            unsigned long * addr = reinterpret_cast<unsigned long *>(map->chunks()[i]);

            db<NicBuffers>(TRC) << "Chunk[" << i << "] => " << addr << endl;

            unsigned long bytes = *--addr;

            internal_free(addr, bytes);
        }

        // Delete the allocation map from the Heap
        delete map;
    }


private:
    void internal_free(void * ptr, unsigned long bytes) {
        db<NicBuffers>(TRC) << "NonCBuffer::free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if(ptr && (bytes >= sizeof(Element))) {
            Element * e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element * m1, * m2;
            insert_merging(e, &m1, &m2);
        }
    }

    static long * minimum_address;
    static long * maximum_address;
    static const int MAX_FRAGMENTATION = 50;

    void out_of_memory(unsigned long bytes);
};


__END_UTIL

#endif
