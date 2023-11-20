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
    else if (header->type() == ECHO_REPLY) 
    {
        for (WaitingResolutionItem::Element * item = _waiting_reply_queue.head(); item; item = item->next()) 
        {
            if (item->object()->id() == header->id()) 
            {
                // 0 is just an information to match the interface of WaitingResolutionItem
                item->object()->resolve_data(0);
            }
        }
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

    unsigned int ping_id = 0;

    Header * header = new (SYSTEM) Header(ICMP::ECHO, DEFAULT, ping_id, 0);

    // Create an item to wait the resolve of the ping
    WaitingResolutionItem * item = new (SYSTEM) WaitingResolutionItem(ping_id, PING_TIMEOUT); 

    _waiting_reply_queue.insert(item->link());

    // Send the echo
    _ip->send(parameters, header, sizeof(Header));

    // Wait for the item to be resolved or the timeout to be triggered
    item->semaphore()->p();

    _waiting_reply_queue.remove(item->link());

    if (!item->was_resolved()) 
    {
        db<ICMP>(TRC) << "ICMP::Unable to ping =" << dst_address << endl;

        delete item;

        return;
    }

    db<ICMP>(TRC) << "ICMP::Ping completed with " << dst_address << endl;

    delete item;
}

__END_SYS
