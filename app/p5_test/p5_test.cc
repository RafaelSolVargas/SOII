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

    unsigned int FRAG_MTU = ip->fragment_mtu();

    unsigned const int DATA_SIZE = FRAG_MTU;

    unsigned const int DATAGRAMS_QUANT = 5;
    
    char data[DATA_SIZE];

    IP::Address destination_ip = IP::Address("150.162.1.50");

    // Sender IP = 192.168.0.1
    // Receiver IP = 192.168.0.2 
    if ((ip->nic()->address()[5] == 9)) 
    { 
        cout << " I'm the sender " << endl;

        Delay(4000000);

        for (unsigned int i = 0; i < DATAGRAMS_QUANT; i++) {
            memset(data, '0' + i, DATA_SIZE);
            
            data[0] = '0' + 9;
            data[DATA_SIZE - 2] = '0' + 9;
            data[DATA_SIZE - 1] = '\n';
    
            ip->send(IP::SendingParameters(destination_ip, IP::TCP, IP::Priority::HIGH), data, DATA_SIZE);
        }
    }
    else if ((ip->nic()->address()[5] == 8))
    { 
        cout << "I'm the receiver" << endl;

        unsigned int initial_packages = ip->statistics().rx_datagrams; 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->statistics().rx_datagrams < DATAGRAMS_QUANT + initial_packages) {
            Delay(100000);
        }
    }  
    else if ((ip->nic()->address()[5] == 7)) {
        cout << "I'm the gateway" << endl;

        unsigned int initial_packages = ip->statistics().rx_datagrams; 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->statistics().rx_datagrams < DATAGRAMS_QUANT + initial_packages) {
            Delay(100000);
        }
    }

    cout << "Finished, now in loop" << endl;
    Delay(5000000);
}


void ip_test_fragmentation() {
    SiFiveU_NIC * nic = SiFiveU_NIC::init();
    IP * ip = IP::init(nic);

    unsigned int FRAG_MTU = ip->fragment_mtu();

    unsigned const int DATA_SIZE = FRAG_MTU * 5 - 200;
    
    char data[DATA_SIZE];

    IP::Address destination_ip = IP::Address("150.162.1.50");
    
    // Sender IP = 192.168.0.1
    // Receiver IP = 192.168.0.2 
    if ((ip->nic()->address()[5] == 9)) 
    { 
        cout << " I'm the sender " << endl;

        Delay(4000000);

        memset(data, '0' + 5, DATA_SIZE);
        
        memset(data + (FRAG_MTU * 0), '0' + 9, 1); // 9 no começo
        memset(data + (FRAG_MTU * 0) + 1, '0' + 1, FRAG_MTU); // Valor no meio
        memset(data + (FRAG_MTU * 1) - 1, '0' + 9, 1); // 9 no final

        memset(data + (FRAG_MTU * 1), '0' + 9, 1); // 9 no começo
        memset(data + (FRAG_MTU * 1) + 1, '0' + 2, FRAG_MTU); // Valor no meio
        memset(data + (FRAG_MTU * 2) - 1, '0' + 9, 1); // 9 no final

        memset(data + (FRAG_MTU * 2), '0' + 9, 1); // 9 no começo
        memset(data + (FRAG_MTU * 2) + 1, '0' + 3, FRAG_MTU); // Valor no meio
        memset(data + (FRAG_MTU * 3) - 1, '0' + 9, 1); // 9 no final

        memset(data + (FRAG_MTU * 3), '0' + 9, 1); // 9 no começo
        memset(data + (FRAG_MTU * 3) + 1, '0' + 4, FRAG_MTU); // Valor no meio
        memset(data + (FRAG_MTU * 4) - 1, '0' + 9, 1); // 9 no final

        memset(data + DATA_SIZE - 1, '0' + 9, 1); // 9 no final

        data[DATA_SIZE - 2] = '0' + 9;
        data[DATA_SIZE - 1] = '\n';

        ip->send(IP::SendingParameters(destination_ip, IP::TCP, IP::Priority::HIGH), data, DATA_SIZE);

        while (ip->statistics().tx_datagrams < 1) {
            Delay(100000);
        }
    }
    else if ((ip->nic()->address()[5] == 8))
    { 
        cout << "I'm the receiver" << endl;

        unsigned int initial_packages = ip->statistics().rx_datagrams; 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->statistics().rx_datagrams < 1 + initial_packages) {
            Delay(100000);
        }
    }  
    else if ((ip->nic()->address()[5] == 7)) {
        cout << "I'm the gateway" << endl;

        unsigned int initial_packages = ip->statistics().rx_datagrams; 

        cout << "Waiting for packages, started with "  << initial_packages << " packages already received" << endl;

        while (ip->statistics().rx_datagrams < 1 + initial_packages) {
            Delay(100000);
        }
    }
}

int main()
{
    //cout << "IP Test Datagram" << endl;

    //ip_test_datagram();

    //cout << "IP Test Datagram Finished" << endl;

    cout << "IP Test Fragmentation" << endl;

    ip_test_fragmentation();

    cout << "IP Test Fragmentation Finished" << endl;

    SiFiveU_NIC * nic = SiFiveU_NIC::init();

    IP * ip = IP::init(nic);

    delete ip;
    delete nic;

    return 0;
}