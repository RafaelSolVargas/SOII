// EPOS NIC Test Programs

#include <machine/nic.h>
#include <time.h>
#include <utility/random.h>
#include <network/ethernet.h>
#include <machine/riscv/sifive_u/sifiveu_nic.h>

using namespace EPOS;

OStream cout;

void ethernet_test() {
    cout << "Ethernet Test" << endl;

    NIC<Ethernet> * nic = Traits<Ethernet>::DEVICES::Get<0>::Result::get();

    NIC<Ethernet>::Address src, dst;
    char data[nic->mtu()];

    NIC<Ethernet>::Address self = nic->address();
    cout << "  MAC: " << self << endl;

    if(self[5] % 2) { // sender
        cout << "I'm the sender" << endl;

        for(int i = 0; i < 10; i++) {
            cout << "Sending frame " << i << endl << endl;

            unsigned long size = nic->mtu() - (sizeof(long) * i);

            memset(data, '0' + i, size);

            data[nic->mtu() - 1] = '\n';
            
            nic->send(nic->broadcast(), 0x8888, data, size);
        }
    } else { // receiver
        cout << "I'm the receiver" << endl;

        while (nic->statistics().rx_packets < 10) {
            Delay(100000);
        }
    }

    NIC<Ethernet>::Statistics stat = nic->statistics();
    cout << "Statistics\n"
         << "Tx Packets: " << stat.tx_packets << "\n"
         << "Tx Bytes:   " << stat.tx_bytes << "\n"
         << "Rx Packets: " << stat.rx_packets << "\n"
         << "Rx Bytes:   " << stat.rx_bytes << "\n";
}


int main()
{
    cout << "NIC Test" << endl;

    #ifdef __ethernet__
        ethernet_test();
    #else
        cout << "Ethernet not configured, skipping test!" << endl;
    #endif


    return 0;
}