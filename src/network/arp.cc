#include <network/arp.h>
#include "synchronizer.h"

__BEGIN_SYS

void ARP::class_nic_callback(BufferInfo * bufferInfo) 
{
    _arp->nic_callback(bufferInfo);
}

void ARP::nic_callback(BufferInfo * bufferInfo) 
{
    Message * message = reinterpret_cast<Message *>(bufferInfo->data());

    if (message->src_mac_addr() == _mac_address) 
    {
        db<ARP>(TRC) << "ARP::DroppingMessage(m=" << *message << ")" << endl;

        return;
    }

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

ARP::MAC_Address ARP::resolve(const Net_Address & searched_address) 
{
    TableEntry::Element * el = _resolutionTable.search_key(searched_address);

    // If exists in table return the cached
    if (el) 
    {
        db<ARP>(TRC) << "ARP::Resolved from table: (" << searched_address << " == " <<  el->object()->mac_address() << ")" << endl;

        return el->object()->mac_address();
    }

    db<ARP>(TRC) << "ARP::Resolving(addr=" << searched_address << ")" << endl;

    for(unsigned int i = 0; i < ARP_TRIES; i++) 
    {
        db<ARP>(TRC) << "ARP::SendingMessage " << i + 1 << endl;
    
        // Create and waiting item
        WaitingResolutionItem * waitingItem = new (SYSTEM) WaitingResolutionItem(searched_address);

        // Insert into queue
        _resolutionQueue.insert(waitingItem->link());

        // Request for an answer
        send_request(searched_address);

        // Wait in semaphore
        waitingItem->semaphore()->p();

        // If not resolved the timeout has being reached, try again
        if (!waitingItem->was_resolved()) 
        {
            _resolutionQueue.remove(waitingItem->link());

            delete waitingItem;

            continue;
        }

        MAC_Address response_address = waitingItem->response_address();
        
        _resolutionQueue.remove(waitingItem->link());

        delete waitingItem;

        db<ARP>(TRC) << "ARP::Resolved from ARP(" << searched_address << " == " <<  response_address << ")" << endl;

        return response_address;
    }

    db<ARP>(TRC) << "ARP::UnableToResolve(a=" << searched_address << ")" << endl;

    return MAC_Address::NULL;
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
    // Saves in table
    TableEntry * table_entry = new (SYSTEM) TableEntry(request->src_net_addr(), request->src_mac_addr());

    _resolutionTable.insert(table_entry->link());

    // If i'm the destiny address, respond with my MAC Address
    if (request->dst_net_addr() == _net_address) 
    {
        NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(request->src_mac_addr(), PROTOCOL, sizeof(Message));

        // Create an reply inside the buffer
        new (buffer->data()) Message(REPLY, _mac_address, _net_address, request->src_mac_addr(), request->src_net_addr());

        Message message = Message(REPLY, _mac_address, _net_address, request->src_mac_addr(), request->src_net_addr());

        db<ARP>(TRC) << "ARP::Answering Request with " << _mac_address << "::Message => " << message << endl;

        _nic->send(buffer);
    } 
    else 
    {
        db<ARP>(TRC) << "ARP::NotDestiny(Dst=" << request->dst_net_addr() << ". My=" << _net_address << ")" << endl;
    }
}

void ARP::handle_reply(Message * message) 
{
    // Saves in table
    TableEntry * table_entry = new (SYSTEM) TableEntry(message->src_net_addr(), message->src_mac_addr());
 
    _resolutionTable.insert(table_entry->link());
    
    // Looks up all items that are waiting for answers and respond, the items will be removed from the queue by the Thread that inserted them
    for (WaitingResolutionItem::Element * item = _resolutionQueue.head(); item; item = item->next()) 
    {
        if (item->object()->searched_address() == message->src_net_addr()) 
        {
            item->object()->resolve_address(message->src_mac_addr());
        }
    }
}

__END_SYS
