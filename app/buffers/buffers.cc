#include <utility/ostream.h>
#include <utility/nic_buffers.h>
#include <system.h>
#include <buffers_handler.h>

using namespace EPOS;

OStream cout;

long* allocate_numbers_in_heap(int id, int quant) 
{
    long* numbers = new long[quant];

    for (long i = 0; i < quant; i++) 
    {
        numbers[i] = (i + id);
    }

    cout << endl;

    cout << "allocate_numbers_in_heap(quant=" << quant << ") => " << numbers << endl;

    return numbers;
}

void print_longs(long * ptr, int size) 
{
    for (int i = 0; i < size; i++) 
    {
        cout << ptr[i] << ",";
    }

    cout << endl;
}

void test_contiguous_buffer() 
{
    BuffersHandler<long> buffer;

    const int PKG_1_SIZE = 10;
    const int PKG_2_SIZE = 5;
    const int PKG_3_SIZE = 8;
    
    long* numbers1 = allocate_numbers_in_heap(13, PKG_1_SIZE);
    long* numbers2 = allocate_numbers_in_heap(45, PKG_2_SIZE);
    long* numbers3 = allocate_numbers_in_heap(68, PKG_3_SIZE);

    cout << "Printing Number1 from Heap" << endl;
    print_longs(numbers1, PKG_1_SIZE);

    cout << "Moving data from Heap do Buffer" << endl;
    void* addr1 = buffer.alloc_c(numbers1, PKG_1_SIZE);
    void* addr2 = buffer.alloc_c(numbers2, PKG_2_SIZE);
    void* addr3 = buffer.alloc_c(numbers3, PKG_3_SIZE);

    cout << "Deleting numbers1 from Heap" << endl;
    delete numbers1;

    cout << "Moving data from Buffer to Heap" << endl;
    long* numbers4 = new long[PKG_1_SIZE];

    buffer.copy_c(addr1, numbers4, PKG_1_SIZE * sizeof(long));

    cout << "Values must be equal to the Number1, previous deallocated from Heap" << endl;
    print_longs(numbers4, PKG_1_SIZE);

    buffer.free_c(addr1);
    buffer.free_c(addr2);
    buffer.free_c(addr3);

    delete[] numbers4;
}

void test_non_contiguous_buffer() 
{
    // The NCBuffer is configured with 64Kb, 8192 longs could be allocated in it.
    BuffersHandler<long> buffer;

    const int PKG_1_SIZE = 125;
    const int PKG_2_SIZE = 7925;
    const int PKG_3_SIZE = 250;

    cout << "Allocating numbers in Heap" << endl;
    long* numbers1 = allocate_numbers_in_heap(13, PKG_1_SIZE);
    long* numbers2 = allocate_numbers_in_heap(500, PKG_2_SIZE);
    long* numbers3 = allocate_numbers_in_heap(0, PKG_3_SIZE);

    cout << "Moving data from PKG_1 and PKG_2 to Buffer" << endl;
    void* addr1 = buffer.alloc_nc(numbers1, PKG_1_SIZE);
    void* addr2 = buffer.alloc_nc(numbers2, PKG_2_SIZE);

    cout << "Deallocating the PKG_1 from the Buffer" << endl;
    buffer.free_nc(addr1);

    cout << "Forcing fragmentation of PKG_3 in the Buffer" << endl;
    void* addr3 = buffer.alloc_nc(numbers3, PKG_3_SIZE);

    cout << "Allocating of size PKG_3 in the Heap" << endl;
    long *numbers4 = allocate_numbers_in_heap(250, PKG_3_SIZE);

    cout << "Printing numbers in numbers4 in the Heap" << endl;
    print_longs(numbers4, PKG_3_SIZE); 

    cout << "Moving fragmented data from PKG_3 to numbers4" << endl;
    buffer.copy_nc(addr3, numbers4, PKG_3_SIZE * sizeof(long));

    cout << "Printing numbers in numbers4 in the Heap, must be 0 to 250, the data from the Buffer" << endl;
    print_longs(numbers4, PKG_3_SIZE); 

    cout << "Moving fragmented data from PKG_2 to numbers4, only half of PKG_3 size" << endl;
    buffer.copy_nc(addr2, numbers4, (PKG_3_SIZE * sizeof(long)) / 2);

    cout << "Printing numbers in numbers4 in the Heap, the first half numbers must start with 500, the data from PKG_2" << endl;
    print_longs(numbers4, PKG_3_SIZE);

    buffer.free_nc(addr2);
    buffer.free_nc(addr3);

    delete[] (numbers1);
    delete[] (numbers2);
    delete[] (numbers3);
    delete[] (numbers4);
}

void test_copying() 
{
    BuffersHandler<long> buffer;

    long* numbers1 = new long[10] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    long* numbers2 = new long[10] {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    
    cout << "Numbers 1 Heap Address => " << numbers1 << endl;
    cout << "Numbers 2 Heap Address => " << numbers2 << endl;

    cout << "Allocating numbers in Buffer" << endl;
    void* addr1 = buffer.alloc_nc(numbers1, 10);
    void* addr2 = buffer.alloc_nc(numbers2, 10);

    print_longs(numbers1, 10);
    print_longs(numbers2, 10);

    cout << "Copying data from numbers1 in Buffer to numbers2 in Heap" << endl;
    buffer.copy_nc(addr1, numbers2, 10 * sizeof(long));

    cout << "Printing both number1 and number2, must be equal " << endl;
    print_longs(numbers1, 10);
    print_longs(numbers2, 10);

    buffer.free_nc(addr1);
    buffer.free_nc(addr2);
}

int main()
{
    cout << ">>>>>>>>>>>>>>> Testing Contiguous Buffer" << endl;
    test_contiguous_buffer();
    
    cout << ">>>>>>>>>>>>>>> Testing Non Contiguous Buffer" << endl;
    test_non_contiguous_buffer();

    cout << ">>>>>>>>>>>>>>> Testing Data Copy" << endl;
    test_copying();

    cout << ">>>>>>>>>>>>>>> Tests finished" << endl;

    return 0;
}
