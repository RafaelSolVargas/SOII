#include <network/icmp.h>

__BEGIN_SYS

ICMP * ICMP::_instance;

ICMP::ICMP(IP * ip) : _ip(ip)
{
    _ip->attach_callback(&class_datagram_callback, IP::ICMP);
    _instance = this;
}

void ICMP::class_datagram_callback(IPHeader * header, AllocationMap * map) 
{
    if (_instance) 
    {
        _instance->datagram_callback(header, map);
    }
}

void ICMP::datagram_callback(IPHeader * ip_header, AllocationMap * map) 
{
    BuffersHandler<char> buff_handler;
    
    Header * header = new (SYSTEM) Header();

    buff_handler.copy_to_mem(map, header, sizeof(Header));

    db<ICMP>(TRC) << "ICMP::PacketReceived=" << *header << endl;

    if (header->type() == ECHO) 
    {
        answer_ping(ip_header, header);
    }

    delete header;
}

void ICMP::answer_ping(IPHeader * ipHeader, Header * pingHeader) 
{
    IP::SendingParameters parameters = IP::SendingParameters(ipHeader->source(), IP::ICMP, IP::HIGH);

    Header * header = new (SYSTEM) Header(ICMP::ECHO_REPLY, DEFAULT, pingHeader->id(), pingHeader->sequence());

    _ip->send(parameters, header, sizeof(Header));

    delete header;
}

void ICMP::ping(Address dst_address) 
{
    IP::SendingParameters parameters = IP::SendingParameters(dst_address, IP::ICMP, IP::HIGH);

    Header * header = new (SYSTEM) Header(ICMP::ECHO, DEFAULT, 0, 0);

    _ip->send(parameters, header, sizeof(Header));
}

__END_SYS
