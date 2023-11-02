#include <network/arp.h>

__BEGIN_SYS

ARP* ARP::_arp;

ARP* ARP::init(IP* ip, SiFiveU_NIC * nic) 
{
    if (!_arp) 
    {
        // Initialize the IP
        _arp = new (SYSTEM) ARP(ip, nic);
    }

    return _arp;
}

ARP::ARP(IP* ip, SiFiveU_NIC * nic) : _ip(ip), _nic(nic)
{ 
    db<ARP>(TRC) << "Initializing ARP" << endl;

    _mac_address = _nic->address();
    _net_address = _ip->address();

    // Configure the callback to be called when the NIC receive frames
    _nic->attach_callback(&class_nic_callback, PROTOCOL);
}

__END_SYS
