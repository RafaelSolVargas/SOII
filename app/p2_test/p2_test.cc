// EPOS NIC Test Programs

#include <network/ethernet.h>
#include <utility/random.h>
#include <machine/nic.h>
#include <network/ip.h>
#include <system.h>
#include <time.h>

using namespace EPOS;

OStream cout;

void show_stats() 
{
    SiFiveU_NIC * nic = SiFiveU_NIC::init();

    NIC<Ethernet>::Statistics stat = nic()->statistics();
    cout << "Statistics\n"
         << "Tx Packets: " << stat.tx_packets << "\n"
         << "Tx Bytes:   " << stat.tx_bytes << "\n"
         << "Rx Packets: " << stat.rx_packets << "\n"
         << "Rx Bytes:   " << stat.rx_bytes << "\n";
}

void ip_test_datagram() {
    SiFiveU_NIC * nic = SiFiveU_NIC::init();

    IP * ip = IP::init(nic);

    unsigned const int MTU = IP::MTU_WO_FRAG;
    unsigned const int DATAGRAMS_QUANT = 10;
    
    char data[MTU];

    NIC<Ethernet>::Address destination_mac = ip->nic()->broadcast();
    bool is_sender = (ip->nic()->address()[5] % 2);

    if (is_sender) 
    { 
        cout << " I'm the sender " << endl;

        for (unsigned int i = 0; i < DATAGRAMS_QUANT; i++) {
            memset(data, '0' + i, MTU);
            
            data[0] = '0' + 9;
            data[MTU - 2] = '0' + 9;
            data[MTU - 1] = '\n';
    
            ip->send(destination_mac, data, MTU);
        }
    }
    else
    { 
        cout << "I'm the receiver" << endl;

        unsigned int initial_packages = ip->nic()->statistics().rx_packets; 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->nic()->statistics().rx_packets < DATAGRAMS_QUANT + initial_packages) {
            Delay(100000);
        }
    }  
}



int main()
{
    cout << "IP Test Datagram" << endl;

    ip_test_datagram();

    cout << "IP Fragmentation Test" << endl;

    //ip_fragmentation_test();

    return 0;
}