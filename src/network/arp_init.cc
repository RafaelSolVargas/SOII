#include <network/arp.h>

__BEGIN_SYS

ARP* ARP::_arp;

ARP* ARP::init(SiFiveU_NIC * nic, const Net_Address & address) 
{
    if (!_arp) 
    {
        // Initialize the IP
        _arp = new (SYSTEM) ARP(nic, address);
    }

    return _arp;
}

ARP::ARP(SiFiveU_NIC * nic, const Net_Address & address) : _nic(nic)
{ 
    db<ARP>(TRC) << "Initializing ARP" << endl;

    _mac_address = _nic->address();
    _net_address = address;

    // Configure the callback to be called when the NIC receive frames
    _nic->attach_callback(&class_nic_callback, PROTOCOL);
}

__END_SYS
