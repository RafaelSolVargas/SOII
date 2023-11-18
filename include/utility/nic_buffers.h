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
    typedef MMU::DMA_Buffer DMA_Buffer;
    typedef CPU::Phy_Addr Phy_Addr;

public:
    using Grouping_List<char>::empty;
    using Grouping_List<char>::size;
    using Grouping_List<char>::grouped_size;

    CBuffer() : _locked(false), _created_dma(false) {
        db<NicBuffers>(TRC) << "CBuffer() => " << this << endl;
    }

    CBuffer(unsigned long bytes) : _locked(false), _created_dma(true) {
        validate_bytes(bytes);

        // Increase the size of Element and a long to allow the allocation of previous bytes quant 
        bytes += sizeof(Element) + sizeof(long);

        _dma = new (SYSTEM) DMA_Buffer(bytes);

        _phy_addr = _dma->phy_address();

        constructor_prologue(_phy_addr, bytes);
    }

    CBuffer(void * addr, unsigned long bytes) : _locked(false), _phy_addr(addr), _created_dma(false) {
        validate_bytes(bytes);

        constructor_prologue(addr, bytes);
    }

    ~CBuffer() {
        if (_created_dma) {
            delete _dma;
        }
    }

    bool lock() 
    { 
        return !CPU::tsl(_locked);
    }
    
    void unlock() 
    { 
        _locked = 0;
    }

    /// @brief Defines if a address is between the range of the addresses that belongs to this Buffer
    /// @param ptr address to verify 
    /// @return 
    bool contains_pointer(void * ptr) {
        long * addr = reinterpret_cast<long *>(ptr);
   
        return addr >= _minimum_address && addr < _maximum_address;
    }

    /// @brief Allocate bytes contiguous size of memory in the Buffer, returning the address that was allocated
    /// @param bytes Quant of bytes to be allocated
    /// @return 
    /// @throws out_of_memory if was not found contiguous space for allocate for required bytes.
    void * alloc(unsigned long bytes) {
        db<NicBuffers>(TRC) << "CBuffer::alloc(this=" << this << ",bytes=" << bytes << endl;

        if(!bytes)
            return 0;

        // Increase a long size to be able to store the size 
        bytes += sizeof(long); 

        validate_bytes(bytes);

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

    void * address() {
        return _phy_addr;
    }

    friend Debug & operator<<(Debug & db, const CBuffer & b) {
        db << "{grouped_size=" << b.grouped_size() << ",locked=" << b._locked << ",addr=" << b._phy_addr << "}";
        return db;
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

    void constructor_prologue(void * addr, unsigned long bytes) 
    {
        db<NicBuffers>(TRC) << "CBuffer(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;

        long * addr_long = reinterpret_cast<long *>(addr);

        // Minimum and maximum address that could be returned by alloc()
        _minimum_address = addr_long;
        _maximum_address = addr_long + (bytes / sizeof(long));
        
        internal_free(addr, bytes);

        db<NicBuffers>(INF) << "CBuffer(Min_Address=" << _minimum_address << 
                                ",Max_Address=" << _maximum_address << 
                                ",GroupedSize=" << grouped_size() <<
                                ")" << endl;
    }

    void validate_bytes(unsigned long & bytes) 
    {
        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;
    }

    volatile bool _locked;
    DMA_Buffer * _dma;
    Phy_Addr _phy_addr;
    bool _created_dma;
    long * _minimum_address;
    long * _maximum_address;

    void out_of_memory(unsigned long bytes);
};

// Class to describe the non contiguous allocation that happened in the NonCBuffer
class MemAllocationMap
{
public:
    MemAllocationMap(unsigned long totalSize, int totalChunks)
        : _quant_chunks(totalChunks), _original_size(totalSize), _total_size(totalSize), _data_copied(0)
    {
        _chunks = new void*[totalChunks];
        _chunks_sizes = new unsigned long[totalChunks];
    }

    ~MemAllocationMap()
    {
        delete[] _chunks;
        delete[] _chunks_sizes;
    }

    void log_allocation() 
    {
        db<NicBuffers>(TRC) << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl; 

        db<NicBuffers>(TRC) << "MemAllocationMap(" << this 
                                    << ") chunks_quant=" << _quant_chunks
                                    << endl; 

        for (int i = 0; i < _quant_chunks; i++) 
        {
            db<NicBuffers>(TRC) << "Chunk[" << i << "] => " 
                                        << "Address=" << _chunks[i]
                                        << ", Size=" << _chunks_sizes[i]
                                        << endl; 
        }
        db<NicBuffers>(TRC) << "DataCopied=" << _data_copied << endl; 
        db<NicBuffers>(TRC) << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl; 
    }
    
    /// @brief Pointers for each chunk allocated by the NonCBuffer 
    void** chunks() { return _chunks; }

    /// @brief Size of data in each chunk
    unsigned long* chunks_sizes() { return _chunks_sizes; }
    
    /// @brief Quant of chunks used in the allocation
    int quant_chunks() { return _quant_chunks; }
    
    /// @brief The chunk where the last copy ended 
    int current_chunk_index() 
    {
        unsigned long offset = 0;
        int i = 0;

        // Pass in each chunk calculating where the _data_copied offset is placed 
        for (; i < _quant_chunks; i++) 
        {
            unsigned long size = _chunks_sizes[i];
            
            // Increase the offset with the total size of the chunk
            offset += size;

            // There is still left data in current chunk
            if (offset > _data_copied) 
            {
                break;
            } 

            // Exactly the data in current buffer was copied, so it's the next
            if (offset == _data_copied) 
            {
                i++;
                break;
            }
        }

        return i;
    }

    /// @brief The chunk offset where the last copy ended
    int current_chunk_offset() 
    {
        unsigned long offset = 0;
        unsigned int i = 0;

        unsigned int chunk_index = current_chunk_index();

        // Pass in each chunk until the before of the current, summing the data size in the previous chunks 
        for (; i < chunk_index; i++) 
        {
            unsigned long size = _chunks_sizes[i];
            
            // Increase the offset with the total size of the chunk
            offset += size;

            db<NicBuffers>(TRC) << "AllocationMap::CurOff::AddOffset =>" << offset << endl;
        }

        db<NicBuffers>(TRC) << "AllocationMap::CurOff =>" << _data_copied - offset << endl;

        return _data_copied - offset;
    }

    /// @brief The chunk where the pointer are pointing after an offset
    int index_of_chunk_with_offset(unsigned long offset) 
    {
        int i = 0;

        // Pass in each chunk calculating where the _data_copied offset is placed 
        for (; i < _quant_chunks; i++) 
        {
            unsigned long size = _chunks_sizes[i];
            
            // If the offset will end in the current chunk, return it
            if (size > offset) 
            {
                return i;
            }

            offset -= size;
        }

        return i;
    }

    /// @brief The offset inside the chunk where the pointer are pointing after an offset
    unsigned long offset_inside_chunk_with_offset(unsigned long offset) 
    {
        int i = 0;

        // Pass in each chunk calculating where the _data_copied offset is placed 
        for (; i < _quant_chunks; i++) 
        {
            unsigned long size = _chunks_sizes[i];
            
            // If the offset will end in the current chunk, return it
            if (size > offset) 
            {
                return offset;
            }

            offset -= size;
        }

        return offset;
    }

    /// @brief Original size of the allocation
    unsigned long original_size() { return _original_size; }
    
    /// @brief Total size to consider in this map
    unsigned long total_size() { return _total_size; }

    void total_size(unsigned long new_size) { _total_size = new_size; }

    /// @brief Total data already copied
    unsigned long data_copied() { return _data_copied; }

    /// @brief Increase the counter of copied data with the size argument
    void data_copied(unsigned long size) { _data_copied += size; }

    /// @brief Reset the data already copied, necessary when wanting the copy again
    void reset_data_copied() { _data_copied = 0; }

private:
    // The chunks allocated by the NonCBuffers
    int _quant_chunks;
    void **_chunks;     
    unsigned long *_chunks_sizes;

    unsigned long _original_size;
    unsigned long _total_size;

    /// Variables to deal with copying data in parts, has a memory of what is already copied
    unsigned long _data_copied;
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
        db<NicBuffers>(TRC) << "NonCBuffer() => " << this << endl;
    }

    NonCBuffer(void * addr, unsigned long bytes) {
        db<NicBuffers>(INF) << "NonCBuffer(addr=" << addr << ",bytes=" << bytes << ") => " << this << endl;

        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;

        long * addr_long = reinterpret_cast<long *>(addr);

        // Minimum and maximum address that could be returned by alloc()
        _minimum_address = addr_long;
        _maximum_address = addr_long + (bytes / sizeof(long));
        
        internal_free(addr, bytes);

        db<NicBuffers>(INF) << "NonCBuffer(Min_Address=" << _minimum_address << 
                                         ",Max_Address=" << _maximum_address << 
                                         ",GroupedSize=" << grouped_size() <<
                                         ")" << endl;
    }

    typedef MemAllocationMap AllocationMap;

    /// @brief Defines if a address is between the range of the addresses that belongs to this Buffer
    /// @param ptr address to verify 
    /// @return 
    bool contains_pointer(void * ptr) {
        long * addr = reinterpret_cast<long *>(ptr);
   
        return addr >= _minimum_address && addr < _maximum_address;
    }

    /// @brief Allocate bytes, that could be non contiguous, memory in the Buffer
    /// @param bytes Quant of bytes to be allocated
    /// @return (AllocationMap *) -> Pointer to the Map of the allocated memory
    /// @throws out_of_memory if was not found space for allocate for required bytes.
    AllocationMap * alloc(unsigned long bytes) {
        db<NicBuffers>(TRC) << "NonCBuffer::alloc(bytes= " << bytes << " | available=" << grouped_size() << ")" << endl;
        if(!bytes)
            return 0;

        if(!Traits<CPU>::unaligned_memory_access)
            while((bytes % sizeof(void *)))
                ++bytes;

        // The size will be allocated in the AllocationMap
        // Minimum size is the Element
        if(bytes < sizeof(Element))
            bytes = sizeof(Element);

        // Verify if the Buffer contains space, possibly fragmented for bytes.
        if (grouped_size() < bytes) 
        {
            out_of_memory(bytes);
        }

        unsigned long size = bytes;
        unsigned long allocated_bytes = 0;
        unsigned long sizes_found[MAX_FRAGMENTATION];
        
        Element * elements[MAX_FRAGMENTATION];
        int elements_quant = 0;

        // While doesn't allocated all bytes or the allocation reached too many fragments
        while (allocated_bytes < bytes && elements_quant < MAX_FRAGMENTATION && size >= sizeof(Element)) 
        {
            db<NicBuffers>(TRC) << "NonCBuffer_Allocation >> Searching for " << size << " bytes contiguous" << endl;

            // Tries to find enough space for the current size
            Element * e = search_decrementing(size);
            if(!e) {

                db<NicBuffers>(TRC) << "NonCBuffer_Allocation >> " << size << " bytes not found, decreasing to ";

                // If not find, decrement the contiguous size searched
                size /= 2;

                // Doesn't allow unaligned size
                if(!Traits<CPU>::unaligned_memory_access)
                    while((size % sizeof(void *)))
                        ++size;

                db<NicBuffers>(TRC) << size << " bytes" << endl;

                continue;
            }

            // Update the bytes already allocated
            allocated_bytes += size;
            
            // Store the allocated element and update counter
            elements[elements_quant] = e;
            sizes_found[elements_quant] = size;
            elements_quant++;

            db<NicBuffers>(TRC) << "NonCBuffer_Allocation >> " << size << " found." << endl;
        }

        // If the required bytes was not allocated, free all elements and throw error
        if (allocated_bytes < bytes) 
        {
            for (int i = 0; i < elements_quant; i++) 
            {
                Element * e = elements[i];

                unsigned long e_size = sizes_found[i];

                long * e_addr = reinterpret_cast<long *>(e->object() + e->size());
                
                internal_free(e_addr, e_size);
            }

            out_of_memory(bytes);
        }

        // Create the map and describe the elements allocated
        AllocationMap * map = new AllocationMap(bytes, elements_quant);
        for (int i = 0; i < elements_quant; i++) 
        {
            Element * e = elements[i];

            long * e_addr = reinterpret_cast<long *>(e->object() + e->size());

            map->chunks()[i] = e_addr;

            map->chunks_sizes()[i] = sizes_found[i];
        }

        return map;
    }

    /// @brief Deallocate the previous allocated memory in the Buffer
    /// @param ptr The address returned by the alloc() method
    void free(void * ptr) {
        AllocationMap * map = reinterpret_cast<AllocationMap *>(ptr);

        db<NicBuffers>(TRC) << "NonCBuffer::free(this=" << this << ", Map=" << ptr << ")" << endl;

        // Iterates for all chunks allocated in the map
        for (int i = 0; i < map->quant_chunks(); i++) 
        {
            long * addr = reinterpret_cast<long *>(map->chunks()[i]);

            unsigned long bytes = map->chunks_sizes()[i];

            internal_free(addr, bytes);
        }

        // Delete the allocation map from the Heap
        delete map;
    }


private:
    void internal_free(void * ptr, unsigned long bytes) {
        db<NicBuffers>(TRC) << "NonCBuffer::internal_free(this=" << this << ",ptr=" << ptr << ",bytes=" << bytes << ")" << endl;

        if(ptr && (bytes >= sizeof(Element))) {
            Element * e = new (ptr) Element(reinterpret_cast<char *>(ptr), bytes);
            Element * m1, * m2;
            insert_merging(e, &m1, &m2);
        }
    }

    long * _minimum_address;
    long * _maximum_address;
    static const int MAX_FRAGMENTATION = 50;

    void out_of_memory(unsigned long bytes);
};


__END_UTIL

#endif
