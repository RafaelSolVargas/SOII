#include <utility/ostream.h>
#include <system.h>

using namespace EPOS;

OStream cout;

int main()
{
    int* numbers = new (CONTIGUOUS_BUFFER) int[10];

    cout << "Hello world!" << endl;

    delete[] (numbers);

    return 0;
}
