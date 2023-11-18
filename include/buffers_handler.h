#ifndef __buffers_handler_h
#define __buffers_handler_h

#include <utility/nic_buffers.h>
#include <system.h>

__BEGIN_SYS

template<class T>
class BuffersHandler
{

public:
    typedef NonCBuffer::AllocationMap AllocationMap;

    /// @brief Allocate the data in the buffer contiguous, the ptr returned could be used directly, (e.g with memcpy)
    /// @param ptr The data to be 
    /// @param elements The size of elements, default is 1 for complex objects pointers
    /// @return 
    void * alloc_c(T * ptr, unsigned long elements = 1)
    {
        unsigned long bytes = elements * sizeof(T);

        void * buffer_ptr = Cbuffer()->alloc(bytes);

        memcpy(buffer_ptr, ptr, bytes);

        return buffer_ptr;
    };

    /// @brief Makes a allocation that could be fragmented, the ptr returned could not be used directly, 
    /// must be used only with the interface methods of this class
    /// @param ptr The address of the memory to be allocated
    /// @param elements The quantity of elements to be allocated
    /// @return 
    void * alloc_nc(T * ptr, unsigned long elements = 1) 
    {
        unsigned long bytes = elements * sizeof(T);

        AllocationMap * map = NCbuffer()->alloc(bytes);

        for (int i = 0; i < map->quant_chunks(); i++) 
        {
            T* c_addr = reinterpret_cast<T*>(map->chunks()[i]);

            unsigned long c_size = map->chunks_sizes()[i];

            memcpy(c_addr, ptr, c_size);

            ptr += (c_size / sizeof(T)); 
        }

        map->log_allocation();

        return map;
    };

    /// @brief Makes a allocation that could be fragmented, the ptr returned could not be used directly, 
    /// must be used only with the interface methods of this class
    /// @param ptr The address of the memory to be allocated
    /// @param elements The quantity of elements to be allocated
    /// @return 
    AllocationMap * alloc_nc(unsigned long elements = 1) 
    {
        unsigned long bytes = elements * sizeof(T);

        AllocationMap * map = NCbuffer()->alloc(bytes);

        return map;
    };

    /// @brief Substitutes the memcpy method, copying the memory from the buffer to a destiny address
    /// @param ptr The ptr returned by the alloc_c()
    /// @param destiny The address to be copied
    /// @param bytes The size in bytes to be copied
    void copy_c(void * ptr, void * destiny, unsigned long bytes)
    {
        db<NicBuffers>(TRC) << "BuffersHandler::copy_c(source=" << ptr 
                                << ", destiny=" << destiny
                                << ", size=" << bytes
                                << ")" << endl;

        memcpy(destiny, ptr, bytes);
    };

    /// @brief Substitutes the memcpy method, copying the memory from the buffer to a destiny address, keeps an memory of the 
    /// already data copied of the allocation, allowing an automatically moving pointer
    /// @param ptr The ptr returned by the alloc_nc()
    /// @param destiny The address to be copied
    /// @param bytes The size in bytes to be copied
    /// @param reset Reset the data size already copied of this allocation
    void copy_to_mem(void * ptr, void * destiny, unsigned long bytes, bool reset = false)
    {
        db<NicBuffers>(TRC) << "BuffersHandler::copy_to_mem(ptr=" << ptr << ",destiny=" << destiny << ", bytes=" << bytes << ")" << endl;

        AllocationMap * map = reinterpret_cast<AllocationMap *>(ptr);

        unsigned long copied = 0;

        // Get the current chunk to copy data
        int i = map->current_chunk_index();
        for (; i < map->quant_chunks(); i++) 
        {
            // Get the information of the current chunk
            T* source_addr = reinterpret_cast<T*>(map->chunks()[i]) + map->current_chunk_offset();

            // Get the size stored in the current buffer
            unsigned long size = map->chunks_sizes()[i];
            
            // Get the maximum data between the required to copy and the available in this buffer
            unsigned long size_to_cp = size; 
            if (size_to_cp > bytes) 
            {
                size_to_cp = bytes;
            }

            db<NicBuffers>(TRC) << "BuffersHandler::copy_to_mem::Chunk[" << i << "] - Source Address => " << &source_addr  << endl;
            db<NicBuffers>(TRC) << "BuffersHandler::copy_to_mem::Chunk[" << i << "] - Destiny Address => " << destiny  << endl;
            db<NicBuffers>(TRC) << "BuffersHandler::copy_to_mem::Chunk[" << i << "] - Size To Copy => " << size_to_cp << " bytes" << endl;
            
            // Copy the data
            memcpy(destiny, source_addr, size_to_cp);

            // Increment the memory of already copied data
            map->data_copied(size_to_cp);
            copied += size_to_cp;

            // Increment the pointer to the destiny
            destiny = static_cast<void *>(static_cast<char *>(destiny) + size_to_cp);

            if (copied >= bytes) 
            {
                break;
            }
        }
    };

    /// @brief Substitutes the memcpy method, copying the memory from an source address into the buffer 
    /// @param map The AllocationMap returned by the alloc_nc()
    /// @param source The address to be copied into the buffer
    /// @param bytes The size in bytes to be copied
    /// @param offset The offset to start the copy
    void copy_to_buffer(AllocationMap * map, void * source, unsigned long bytes, unsigned long offset = 0)
    {
        db<NicBuffers>(TRC) << "BuffersHandler::copy_to_buffer(map=" << map << ",source=" << source << ", bytes=" << bytes << ", offset=" << offset << ")" << endl;

        unsigned long copied = 0;

        // Get the current chunk to copy data
        int i = map->index_of_chunk_with_offset(offset);
        unsigned long chunk_offset = map->offset_inside_chunk_with_offset(offset);
        for (; i < map->quant_chunks(); i++) 
        {
            // Get the address of the chunk where the current offset applies 
            T* dst_addr = reinterpret_cast<T*>(map->chunks()[i]) + chunk_offset;

            // Calculates bytes available in the current chunk to copy
            unsigned long size_available_in_chunk = map->chunks_sizes()[i] - chunk_offset;
            
            // Get the maximum data between the required to copy and the available in this buffer
            unsigned long size_to_cp = size_available_in_chunk; 
            if (size_to_cp > bytes) 
            {
                size_to_cp = bytes;
            }

            db<NicBuffers>(TRC) << "BuffersHandler::copy_to_buffer::Chunk[" << i << "] - Source Address => " << source  << endl;
            db<NicBuffers>(TRC) << "BuffersHandler::copy_to_buffer::Chunk[" << i << "] - Destiny Address => " << &dst_addr  << endl;
            db<NicBuffers>(TRC) << "BuffersHandler::copy_to_buffer::Chunk[" << i << "] - Size To Copy => " << size_to_cp << " bytes" << endl;
            
            // Copy the data
            memcpy(dst_addr, source, size_to_cp);

            // Increment the memory of already copied data
            copied += size_to_cp;

            // Increment the pointer of the source
            source = static_cast<void *>(static_cast<char *>(source) + size_to_cp);

            // If already copied all bytes, break
            if (copied >= bytes) 
            {
                break;
            }

            // Reset the chunk offset if necessary to go to another chunk
            chunk_offset = 0;
        }
    };

    /// @brief Deallocate the previous contiguous data allocated in the Buffer
    /// @param ptr The ptr returned from the alloc_c()
    void free_c(void * ptr)
    {
        Cbuffer()->free(ptr);
    };

    /// @brief Deallocate the previous non contiguous data allocated in the Buffer
    /// @param ptr The ptr returned from the alloc_nc()
    void free_nc(void * ptr)
    {
        NCbuffer()->free(ptr);
    };

private:
    static CBuffer * Cbuffer() { return System::_Cbuffer; }
    static NonCBuffer * NCbuffer() { return System::_NCbuffer; }
};

__END_SYS

#endif