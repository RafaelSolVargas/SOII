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
        _ip = new (SYSTEM) IP(nic);
    }

    return _ip;
}

IP::IP(NIC<Ethernet> * nic) : _nic(reinterpret_cast<SiFiveU_NIC*>(nic)) { }

__END_SYS
