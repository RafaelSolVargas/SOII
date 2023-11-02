#include <network/arp.h>

__BEGIN_SYS

ARP::MAC_Address ARP::resolve(Net_Address net_address) 
{
    return _mac_address;
}


void ARP::class_nic_callback(BufferInfo * bufferInfo) 
{
    _arp->nic_callback(bufferInfo);
}

void ARP::nic_callback(BufferInfo * bufferInfo) 
{

}

__END_SYS
