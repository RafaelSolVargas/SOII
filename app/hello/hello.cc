#include <utility/ostream.h>
#include <network/ethernet.h>
#include <machine/riscv/sifive_u/sifiveu_nic.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Hello world!" << endl;

    SiFiveU_NIC * my_nic = System::_nic;
    
    NIC<Ethernet>::Address mac = my_nic->address();

    cout << "MAC: " << mac << endl;

    return 0;
}
