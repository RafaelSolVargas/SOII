#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <utility/ostream.h>
#include <utility/nic_buffers.h>
#include <network/ethernet.h>
#include <machine/nic.h>
#include <time.h>
#include <system.h>

using namespace EPOS;

OStream cout;


int main()
{
    SiFiveU_NIC * nic = System::_nic;

    SiFiveU_NIC::Address src, dst;
    SiFiveU_NIC::Protocol prot;
    char data[nic->mtu()];

    cout << "NIC MTU =>" << nic->mtu() << endl;

    SiFiveU_NIC::Address self = nic->address();
    cout << "MAC: " << self << endl;

    if (true) { //if(self[5] % 2) { // sender
        for(int i = 0; i < 5; i++) {
            cout << "Sending Operation " << i << endl;

            memset(data, '0' + i, nic->mtu());

            data[nic->mtu() - 1] = '\n';
            
            nic->send(nic->broadcast(), 0x8888, data, nic->mtu());
        }
    } else { // receiver
        for(int i = 0; i < 5; i++) {
           nic->receive(&src, &prot, data, nic->mtu());

           cout << "  Data: " << data;
        }
    }

    return 0;
}
