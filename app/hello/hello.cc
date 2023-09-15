#include <utility/ostream.h>

using namespace EPOS;

OStream cout;

int main()
{
    volatile unsigned int y = *reinterpret_cast<volatile unsigned int*>(0x10090008);

    cout << "Address 0x10090008 = " << y << endl;

    cout << "Hello world!" << endl;

    return 0;
}
