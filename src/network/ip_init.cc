#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <network/ip.h>
#include <machine/nic.h>
#include <system.h>
#include <memory.h>
#include <machine.h>

__BEGIN_SYS

unsigned int IP::_datagram_count = 0;
IP* IP::_ip;

IP* IP::init(NIC<Ethernet> * nic) 
{
    if (!_ip) 
    {
        // Initialize the IP
        _ip = new (SYSTEM) IP(nic);

        // Configure the callback to be called when the NIC receive frames
        _ip->_nic->attach_callback(&class_nic_callback, IP_PROT);
    }

    return _ip;
}

IP::IP(NIC<Ethernet> * nic) : _nic(reinterpret_cast<SiFiveU_NIC*>(nic)) { }

__END_SYS
