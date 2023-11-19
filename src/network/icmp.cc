#include <network/icmp.h>

__BEGIN_SYS

ICMP::ICMP(IP * ip) : _ip(ip)
{
    _ip->attach_callback(&datagram_callback, IP::ICMP);
}

void ICMP::datagram_callback(IPHeader * header, AllocationMap * map) 
{

}

__END_SYS
