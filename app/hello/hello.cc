#include <utility/ostream.h>
#include <system.h>
#include <utility/nic_buffers.h>

using namespace EPOS;

OStream cout;


long* allocate_numbers(int id, int quant) 
{
    long* numbers = new long[quant];

    cout << "Allocating Frame " << id << " with " << quant << " numbers in Heap. Addr => (" << numbers << ")" << endl;

    for (long i = 0; i < quant; i++) 
    {
        numbers[i] = (i + 1) * 100 + id;
    }

    numbers[0] = id;

    return numbers;
}

void * send_non_c(void * ptr, unsigned long size) 
{
    NonCBuffer::AllocationMap * buffer_ptr = _SYS::System::NCbuffer()->alloc(size);

    cout << "Buffering Ptr=(" << ptr << ") of Size=" << size << ". Addr => " << buffer_ptr << endl;
 
    buffer_ptr->log_allocation();

    return buffer_ptr;
}

void free_pkg_non_c(void * ptr) 
{
    _SYS::System::NCbuffer()->free(ptr);
}

void * send(void * ptr, unsigned long size) 
{
    void * buffer_ptr = _SYS::System::NCbuffer()->alloc(size);

    cout << "Buffering Ptr=(" << ptr << ") of Size=" << size << ". Addr => " << buffer_ptr << endl;
 
    memcpy(buffer_ptr, ptr, size);

    return buffer_ptr;
}

void receive(void * ptr, int quant) 
{
    long* numbers = reinterpret_cast<long*>(ptr);

    cout << "Reading numbers from Cbuffer, allocated at =>" << numbers << endl;

    // Preencher o array com valores sequenciais
    for (long i = 0; i < quant; i++) {
        // cout << numbers[i] << ",";
    }

    cout << endl;
}

int main()
{
    // Buffer de 65536 Kb cabe 8192 longs
    const int PKG_1_SIZE = 4000;
    const int PKG_2_SIZE = 2000;
    const int PKG_3_SIZE = 500;
    const int PKG_4_SIZE = 1000;
    // Após alocar os anteriores, desalocar o PKG_3 e alocar o PKG_5 para forçar uma fragmentação nele
    const int PKG_5_SIZE = 1000;
    
    long* numbers1 = allocate_numbers(1, PKG_1_SIZE);
    long* numbers2 = allocate_numbers(2, PKG_2_SIZE);
    long* numbers3 = allocate_numbers(3, PKG_3_SIZE);
    long* numbers4 = allocate_numbers(4, PKG_4_SIZE);
    long* numbers5 = allocate_numbers(5, PKG_5_SIZE);

    void * ptr1 = send_non_c(numbers1, PKG_1_SIZE * 8);
    
    cout << ptr1 << endl;
    free_pkg_non_c(ptr1);

    void * ptr2 = send_non_c(numbers2, PKG_2_SIZE * 8);
    void * ptr3 = send_non_c(numbers3, PKG_3_SIZE * 8);
    void * ptr4 = send_non_c(numbers4, PKG_4_SIZE * 8);


    void * ptr5 = send_non_c(numbers5, PKG_5_SIZE * 8);

    cout << "Deleting numbers from heap" << endl;
    delete[] (numbers1);
    delete[] (numbers2);
    delete[] (numbers3);
    delete[] (numbers4);
    delete[] (numbers5);

    receive(ptr1, PKG_1_SIZE);
    receive(ptr2, PKG_2_SIZE);
    receive(ptr3, PKG_3_SIZE);
    receive(ptr4, PKG_4_SIZE);
    receive(ptr5, PKG_5_SIZE);

    cout << "Hello world!" << endl;

    return 0;
}
