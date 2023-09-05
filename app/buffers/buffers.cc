#include <utility/ostream.h>
#include <system.h>
#include <utility/nic_buffers.h>

using namespace EPOS;

OStream cout;


long* allocate_numbers_in_heap(int id, int quant) 
{
    long* numbers = new long[quant];

    for (long i = 0; i < quant; i++) 
    {
        numbers[i] = (i + id);

        cout << numbers[i] << ",";
    }

    cout << endl;

    return numbers;
}

void * send_non_contiguous(long * mem_ptr, unsigned long size) 
{
    NonCBuffer::AllocationMap * map = _SYS::System::NCbuffer()->alloc(size);

    for (int i = 0; i < map->quant_chunks(); i++) 
    {
        long* buffer_addr = reinterpret_cast<long*>(map->chunks()[i]);

        unsigned long buffer_size = map->chunks_sizes()[i];

        memcpy(buffer_addr, mem_ptr, buffer_size);

        mem_ptr += (buffer_size / sizeof(long)); 
    }

    map->log_allocation();

    return map;
}

void free_pkg_non_c(void * ptr) 
{
    _SYS::System::NCbuffer()->free(ptr);
}

void * send_contiguous(void * ptr, unsigned long size) 
{
    void * buffer_ptr = _SYS::System::Cbuffer()->alloc(size);

    memcpy(buffer_ptr, ptr, size);

    return buffer_ptr;
}

void receive(void * ptr, int quant) 
{
    NonCBuffer::AllocationMap * map = reinterpret_cast<NonCBuffer::AllocationMap *>(ptr);

    cout << "Reading numbers from Ptr=(" << ptr << ")" << endl;

    for (int i = 0; i < map->quant_chunks(); i++) 
    {
        long* c_addr = reinterpret_cast<long*>(map->chunks()[i]);

        int c_quant = map->chunks_sizes()[i] / sizeof(long);

        for (int ii = 0; ii < c_quant; ii++) 
        {
            cout << *c_addr << ",";  

            c_addr++;  
        }

        cout << endl;
    }
}

int main()
{
    const int PKG_1_SIZE = 1000;
    const int PKG_2_SIZE = 6000;
    const int PKG_3_SIZE = 2000;
    
    long* numbers1 = allocate_numbers_in_heap(1, PKG_1_SIZE);
    long* numbers2 = allocate_numbers_in_heap(2, PKG_2_SIZE);
    long* numbers3 = allocate_numbers_in_heap(3, PKG_3_SIZE);

    void * ptr1 = send_non_contiguous(numbers1, PKG_1_SIZE * 8);
    void * ptr2 = send_non_contiguous(numbers2, PKG_2_SIZE * 8);

    // Deleting the PKG_1 from the Buffer will force a fragmentation in the PKG_3
    free_pkg_non_c(ptr1);
    void * ptr3 = send_non_contiguous(numbers3, PKG_3_SIZE * 8);

    cout << "Deleting numbers from heap" << endl;
    delete[] (numbers1);
    delete[] (numbers2);
    delete[] (numbers3);

    receive(ptr2, PKG_1_SIZE);
    receive(ptr3, PKG_2_SIZE);

    return 0;
}
