// EPOS NIC Test Programs

#include <machine/nic.h>
#include <network/ip.h>
#include <system.h>
#include <time.h>
#include <utility/random.h>
#include <network/ethernet.h>
#include <machine/riscv/sifive_u/sifiveu_nic.h>

using namespace EPOS;

OStream cout;

template<typename Family>
class NIC_Receiver: public NIC<Family>::Observer
{
private:
    void update(typename NIC<Family>::Observed * obs,  const typename NIC<Family>::Protocol & p, typename NIC<Family>::Buffer * buf) {
        typename Family::Frame * frame = buf->frame();
        char * data = frame->template data<char>();
        for(unsigned int i = 0; i < buf->size() - sizeof(typename Family::Header) - sizeof(typename Family::Trailer); i++)
            cout << data[i];
        cout << "\n" << endl;
        buf->nic()->free(buf); // to return to the buffer pool;
    }
};


void ethernet_test() {
    cout << "Ethernet Test" << endl;

    IP * ip = System::_ip;

    unsigned int mtu = ip->MTU_WO_FRAG;
    /*unsigned int pkg_quant = 1;
    unsigned int data_size = mtu * pkg_quant;
    char data[data_size]; // MTU * 10 */

    NIC<Ethernet> * nic = ip->nic();
    NIC<Ethernet>::Address dst;
    NIC<Ethernet>::Address self = nic->address();
    cout << "  MAC: " << self << endl;

    if(self[5] % 2) { // sender
        char data[mtu];

        memset(data, '0' + 5, mtu);
        
        data[mtu - 1] = '\n';
        data[mtu - 2] = '9';
        data[0] = '9';

        cout << "Data:" << data << endl;

        ip->send(nic->broadcast(), data, mtu);
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


#ifdef __ieee802_15_4__

void ieee802_15_4_test() {
    cout << "IEEE 802.15.4 Test" << endl;

    NIC<IEEE802_15_4> * nic = Traits<IEEE802_15_4>::DEVICES::Get<0>::Result::get(0);

    NIC_Receiver<IEEE802_15_4> * receiver = new NIC_Receiver<IEEE802_15_4>;
    nic->attach(receiver, IEEE802_15_4::PROTO_ELP);

    char original[] = "000 ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890\0";
    char data[150];

    NIC<IEEE802_15_4>::Address self = nic->address();
    cout << "  MAC: " << self << endl;

    int pct = 0;

    while(true) {
        for(int i = 0; i < 10; i++) {
            int rnd = Random::random();
            if(rnd < 0)
                rnd = -rnd;

            int size = 12 + (rnd % 100);
            for(int p = 0; p < size; p++)
                data[p] = original[p];
            data[size-3] = '='; // to easily verify if received correctly
            data[size-2] = 'X'; // Is sent raw, the last 2 bytes are CRC, must be included in "Packet", and
            data[size-1] = 'X'; // will be discarded at receiver.
            data[0] = '0' + pct / 100;
            data[1] = '0' + (pct % 100) / 10;
            data[2] = '0' + pct % 10;
            pct = (pct + 1) % 1000;

            nic->send(nic->broadcast(), IEEE802_15_4::PROTO_ELP, data, size);
            Alarm::delay(1000000);
        }

        NIC<IEEE802_15_4>::Statistics stat = nic->statistics();
        cout << "Statistics\n"
             << "Tx Packets: " << stat.tx_packets << "\n"
             << "Tx Bytes:   " << stat.tx_bytes << "\n"
             << "Rx Packets: " << stat.rx_packets << "\n"
             << "Rx Bytes:   " << stat.rx_bytes << "\n";
    }
}

#endif

int main()
{
    cout << "NIC Test" << endl;

#ifdef __ethernet__
    ethernet_test();
#else
    cout << "Ethernet not configured, skipping test!" << endl;
#endif

#ifdef __ieee802_15_4__
    ieee802_15_4_test();
#else
    cout << "IEEE 802.15.4 not configured, skipping test!" << endl;
#endif

    return 0;
}
