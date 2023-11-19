#ifndef __tcp_h
#define __tcp_h

#include "network/ip.h"

__BEGIN_SYS

class TCP 
{
private:
    typedef IP::AllocationMap AllocationMap;
    typedef IP::Header IPHeader;

public:
    static TCP * init(IP * ip);

    static void class_handle_datagram(IPHeader * header, AllocationMap * map);

    void handle_datagram(IPHeader * header, AllocationMap * map);

protected:
    TCP(IP * ip) : _ip(ip) 
    {
        _ip->attach_callback(&class_handle_datagram, IP::TCP);
    }

private:
    BuffersHandler<char> nw_buffers;

    static TCP * _instance;
    IP * _ip;
};

__END_SYS

#endif
