#include <network/ethernet.h>
#include <network/ip.h>
#include <buffers_handler.h>
#include <system.h>

__BEGIN_SYS

void IP::send(SendingParameters parameters, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send(parameters= " << parameters << ",size=" << size << ",data=" << data << ")" << endl;

    // If the data fits in an fragment, send it directly
    if (size <= Header::FRAGMENT_MTU) 
    {
        unsigned int id = get_next_id();

        send_data(parameters.destiny, parameters.protocol, id, FragmentFlags::DATAGRAM, data, size);

        return;
    }

    // Copy the data into an NonCBuffer and send it
    void * buffered_data_ptr = nw_buffers.alloc_nc(const_cast<char*>(reinterpret_cast<const char*>(data)), size);
    
    // TODO -> Create an queue and a Thread to send the data and return it before actually sending to the NIC
    send_buffered_data(parameters, buffered_data_ptr);
}

void IP::send_buffered_data(SendingParameters parameters, void * buffer_ptr) 
{   
    db<IP>(TRC) << "IP::send_buffered_data(parameters= " << parameters << ",buffer_ptr=" << buffer_ptr << ")" << endl;

    AllocationMap * map = reinterpret_cast<AllocationMap *>(buffer_ptr);

    unsigned long total_size = map->original_size();

    unsigned int id = get_next_id();

    // If the data allocated in fits in and is only one chunk send it directly
    if (total_size <= Header::FRAGMENT_MTU && map->quant_chunks() == 1) 
    {
        void * data_ptr = map->chunks()[0];

        unsigned long data_size = map->chunks_sizes()[0];

        send_data(parameters.destiny, parameters.protocol, id, FragmentFlags::DATAGRAM, data_ptr, data_size);
    
        return;
    }
    // Fits in one Fragment but it's required to use the BuffersHandler interface
    else if (total_size <= Header::FRAGMENT_MTU) 
    {
        // Allocate an NIC Buffer and create the Header inside it without the data
        NIC<Ethernet>::BufferInfo * buffer = prepare_header_in_nic_buffer(parameters.destiny, parameters.protocol, id, FragmentFlags::DATAGRAM, total_size, total_size, 0);

        // Copy the data to the Header
        Header * header = reinterpret_cast<Header *>(buffer->data());

        // Use the interface to copy the chunks of the allocation map to the Buffer
        nw_buffers.copy_nc(buffer_ptr, header->data_address(), total_size);

        _nic->send(buffer);

        return;
    }
    // Fragmentation will be required 
    else
    {
        DatagramBufferedRX* datagram_buffered = new (SYSTEM) DatagramBufferedRX(parameters, map); 

        // Insert into sending queue the information of this Datagram
        _queue.insert(datagram_buffered->link());

        // Releases the Thread to send the Datagram
        _semaphore->v();
    }
}

void IP::send_data(const Address & dst, const Protocol & prot, unsigned int id, FragmentFlags flags, const void * data, unsigned int data_size) 
{
    // Allocate an NIC Buffer and create the Header inside it without the data
    NIC<Ethernet>::BufferInfo * buffer = prepare_header_in_nic_buffer(dst, prot, id, flags, data_size, data_size, 0);

    // Copy the data to the Header
    Header * header = reinterpret_cast<Header *>(buffer->data());
    
    memcpy(header->data_address(), data, data_size);
    
    _nic->send(buffer);
}

void IP::send_buffered_with_fragmentation(const Address & dst, const Protocol & prot, unsigned int id, AllocationMap * map) 
{
    unsigned long total_size = map->original_size();

    db<IP>(TRC) << "IP::Fragmentation => Starting Fragmentation of Datagram [" << id << "] with " << total_size << " bytes of data" << endl;

    unsigned long data_sended = 0;
    unsigned int offset = 0;

    while (data_sended < total_size) 
    {
        bool is_last = data_sended + FRAGMENT_MTU >= total_size;

        FragmentFlags flag = is_last ? FragmentFlags::LAST_FRAGMENT : FragmentFlags::MID_FRAGMENT;

        unsigned int fragment_size = is_last ? total_size - data_sended : FRAGMENT_MTU;

        db<IP>(TRC) << "IP::Fragment[" << offset << "] => " 
                    << " Data_Sended=" << data_sended
                    << " Is_Last=" << is_last
                    << " fragment_size=" << fragment_size << endl;

        BufferInfo * buffer = prepare_header_in_nic_buffer(dst, prot, id, flag, total_size, fragment_size, offset);

        Header * header = reinterpret_cast<Header *>(buffer->data());

        db<IP>(TRC) << "IP::Fragment[" << offset << "]" << " => " << *header << endl;

        nw_buffers.copy_nc(map, header->data_address(), fragment_size);

        _nic->send(buffer);

        // Update data for next loop
        offset++;
        data_sended += fragment_size;
    }
}

NIC<Ethernet>::BufferInfo* IP::prepare_header_in_nic_buffer(const Address & dst, const Protocol & prot, unsigned int id, 
FragmentFlags flags, unsigned int total_size, unsigned int data_size, unsigned int offset) 
{
    // Calculate the required size, adding the space for creating and fragment and a datagram
    unsigned int required_size = data_size + sizeof(Header);

    // Get the MAC Address of the IP Address
    MAC_Address dst_mac = _router->route(dst);

    // Preallocate an buffer to use it for sending the package 
    NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(dst_mac, PROTOCOL, required_size);

    // Create an datagram 
    new (buffer->data()) Header(_address, dst, prot, total_size, id, flags, offset);

    return buffer;
}

int IP::class_sending_queue_function() 
{
    return _ip->sending_queue_function();
}

int IP::sending_queue_function() 
{
    db<IP>(INF) << "IP::SendingQueueThread => Initializing" << endl;

    while (!_deleted) 
    {
        db<IP>(INF) << "IP::SendingQueueThread => Waiting for next Datagram" << endl;

        _semaphore->p();

        if (_deleted) break;

        DatagramBufferedRX * buffered_datagram = _queue.remove()->object();

        SendingParameters config = buffered_datagram->config();

        unsigned int id = get_next_id();

        db<IP>(INF) << "IP::SendingQueueThread => Starting Fragmentation of Datagram " << id << endl;

        send_buffered_with_fragmentation(config.destiny, config.protocol, id, buffered_datagram->map());

        _stats.tx_datagrams++;

        delete buffered_datagram;
    }

    db<IP>(INF) << "IP::~SendingThread()" << endl;

    return 1;
}

unsigned int IP::get_next_id() 
{
    // TODO -> Protect with mutex

    return _ip->_stats.next_id++;
}

__END_SYS
