#include <network/ethernet.h>
#include <network/ip.h>

__BEGIN_SYS

void IP::update(Observed * observed, const Protocol & c, BufferInfo * d)
{
    observed->free(d);
};

void IP::send(Address dst, const void * data, unsigned int size) 
{

    for(int i = 0; i < 10; i++) {
        cout << "Sending frame " << i << endl << endl;

        unsigned long size = _nic->mtu() - (sizeof(long) * i);

        memset(data, '0' + i, size);

        data[nic->mtu() - 1] = '\n';
        
        nic->send(dst, Ethernet::PROTO_IP, data, size);
    }
}

__END_SYS
