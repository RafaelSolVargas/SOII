#include "network/tcp.h"

__BEGIN_SYS

TCP * TCP::_instance;

TCP * TCP::init(IP * ip) 
{
    if (!_instance) 
    {
        _instance = new (SYSTEM) TCP(ip);
    }

    return _instance;
}

void TCP::class_handle_datagram(IPHeader * header, AllocationMap * map) 
{
    if (!_instance) return;

    _instance->handle_datagram(header, map);
}

void TCP::handle_datagram(IPHeader * header, AllocationMap * map) 
{
    unsigned long data_size = header->data_size();

    char data[data_size];

    nw_buffers.copy_to_mem(map, data, data_size, false);

    db<TCP>(TRC) << "TCP::DatagramReceived" << endl;
    db<TCP>(TRC) << data << endl;
};

__END_SYS

