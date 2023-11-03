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
    Message * message = reinterpret_cast<Message *>(bufferInfo->data());

    db<ARP>(TRC) << "ARP::message_received(m=" << *message << ")" << endl;

    if (message->opcode() == REQUEST) 
    {
        respond_request(message);
    }
    else if (message->opcode() == REPLY) 
    {
        handle_reply(message);
    }
}

void ARP::send_request(Net_Address searched_address) 
{
    NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(MAC_Address::BROADCAST, PROTOCOL, sizeof(Message));

    // Create an Request inside the buffer
    new (buffer->data()) Message(REQUEST, _mac_address, _net_address, MAC_Address::NULL, searched_address);
    
    _nic->send(buffer);
}

void ARP::respond_request(Message * request) 
{
    // If i'm the destiny address, respond with my MAC Address
    if (request->dst_net_addr() == _net_address) 
    {
        NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(request->src_mac_addr(), PROTOCOL, sizeof(Message));

        // Create an reply inside the buffer
        new (buffer->data()) Message(REPLY, _mac_address, _net_address, request->src_mac_addr(), request->src_net_addr());
        
        _nic->send(buffer);
    }
}

void ARP::handle_reply(Message * message) 
{

}

__END_SYS
