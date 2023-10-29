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

    NIC<Ethernet>::Statistics stat = nic->statistics();
    cout << "Statistics\n"
         << "Tx Packets: " << stat.tx_packets << "\n"
         << "Tx Bytes:   " << stat.tx_bytes << "\n"
         << "Rx Packets: " << stat.rx_packets << "\n"
         << "Rx Bytes:   " << stat.rx_bytes << "\n";
}

void ip_test_datagram() {
    SiFiveU_NIC * nic = SiFiveU_NIC::init();

    IP * ip = IP::init(nic);

    unsigned const int MTU = ip->fragment_mtu();
    unsigned const int DATAGRAMS_QUANT = 10;
    
    char data[MTU];

    IP::Address destination_ip = ip->broadcast();
    bool is_sender = (ip->address()[3] % 2);

    if (is_sender) 
    { 
        cout << " I'm the sender " << endl;

        Delay(100000);

        for (unsigned int i = 0; i < DATAGRAMS_QUANT; i++) {
            memset(data, '0' + i, MTU);
            
            data[0] = '0' + 9;
            data[MTU - 2] = '0' + 9;
            data[MTU - 1] = '\n';
    
            ip->send(destination_ip, IP::TCP, data, MTU);
        }
    }
    else
    { 
        cout << "I'm the receiver" << endl;

        unsigned int initial_packages = ip->datagrams_received(); 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->datagrams_received() < DATAGRAMS_QUANT + initial_packages) {
            Delay(100000);
        }
    }  
}

void ip_test_fragmentation() {
    SiFiveU_NIC * nic = SiFiveU_NIC::init();
    IP * ip = IP::init(nic);

    unsigned const int DATA_SIZE = ip->fragment_mtu() * 5 - 200;
    
    char data[DATA_SIZE];

    IP::Address destination_ip = ip->broadcast();
    bool is_sender = (ip->address()[3] % 2);

    if (is_sender) 
    { 
        cout << " I'm the sender " << endl;

        Delay(100000);

        memset(data, '0' + 4, DATA_SIZE);
        
        data[0] = '0' + 8;
        data[DATA_SIZE - 2] = '0' + 8;
        data[DATA_SIZE - 1] = '\n';

        ip->send(destination_ip, IP::TCP, data, DATA_SIZE);
    }
    else
    { 
        cout << "I'm the receiver" << endl;

        unsigned int initial_packages = ip->datagrams_received(); 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->datagrams_received() < 1 + initial_packages) {
            Delay(100000);
        }
    }  
}

int main()
{
    cout << "IP Test Datagram" << endl;

    //ip_test_datagram();

    cout << "IP Test Datagram Finished" << endl;

    cout << "IP Test Fragmentation" << endl;

    ip_test_fragmentation();

    cout << "IP Test Fragmentation Finished" << endl;

    SiFiveU_NIC * nic = SiFiveU_NIC::init();

    IP * ip = IP::init(nic);

    delete ip;
    delete nic;

    return 0;
}
