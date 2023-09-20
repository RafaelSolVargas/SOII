#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <utility/ostream.h>
#include <utility/nic_buffers.h>
#include <network/ethernet.h>
#include <machine/nic.h>
#include <system.h>

using namespace EPOS;

OStream cout;


int main()
{
    SiFiveU_NIC * nic = System::_nic;
    Ethernet::Protocol prot = Ethernet::PROTO_IP;
    NIC_Common::Address<6> addr;

    long* numbers1 = new long[10] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    unsigned long size = 10 * sizeof(long);

    nic->send(addr, prot, numbers1, size);

    delete numbers1;

    cout << "Execution completed" << endl;

    return 0;
}
