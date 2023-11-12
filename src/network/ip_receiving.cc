#include <network/ethernet.h>
#include <network/ip.h>
#include <system.h>

__BEGIN_SYS

void IP::class_nic_callback(BufferInfo * bufferInfo) 
{
    _ip->nic_callback(bufferInfo);
}

void IP::nic_callback(BufferInfo * bufferInfo) 
{
    Header * fragment = reinterpret_cast<Header *>(bufferInfo->data());

    db<IP>(TRC) << "IP::receive(frag=" << *fragment << ")" << endl;

    if (fragment->flags() == FragmentFlags::DATAGRAM) 
    {
        handle_datagram(fragment);
    } 
    else 
    {
        handle_fragmentation(fragment);
    }
}

void IP::handle_datagram(Header * datagram) 
{
    if (datagram->destiny() != _address) 
    {
        if (_router->is_gateway()) 
        {
            db<IP>(TRC) << "IP::RoutingDatagram::DST=(" << datagram->destiny() << ")" << endl;

            // Move the data from the NIC Rx Buffer into the IP
            void * buffered_data = nw_buffers.alloc_nc(const_cast<char*>(reinterpret_cast<const char*>(datagram->data_address())), datagram->data_size());

            MemAllocationMap * map = reinterpret_cast<MemAllocationMap*>(buffered_data);

            SendingParameters parameters = SendingParameters(datagram->destiny(), datagram->protocol(), HIGH);

            // Create an Header to be reused when sending it
            Header header_to_reuse = Header(datagram->source(), datagram->destiny(), datagram->protocol(), datagram->data_size(), datagram->id(), DATAGRAM, 0);

            // Insert the datagram in the queue to be sended
            DatagramBufferedRX* datagram_buffered = new (SYSTEM) DatagramBufferedRX(parameters, map, header_to_reuse); 

            // Insert into sending queue the information of this Datagram
            _queue.insert(datagram_buffered->link());

            // Releases the Thread to send the Datagram
            _semaphore->v();

            return;
        }
        else
        {
            db<IP>(TRC) << "IP::Ignoring datagram that is not for me!" << endl;
     
            return;
        }
    }

    unsigned int payload = datagram->data_size();

    char receivedData[payload];

    memcpy(receivedData, datagram->data_address(), payload);

    db<IP>(TRC) << "IP::Datagram Data Received= " << *datagram << endl;

    for (unsigned int index = 0; index < payload; index++) 
    {
        db<IP>(TRC) << receivedData[index];
    }

    _stats.rx_datagrams++;
}

void IP::handle_fragmentation(Header * fragment) 
{
    if (fragment->flags() == FragmentFlags::LAST_FRAGMENT) 
    {

    } 
    else // Mid Fragment
    {

    }
}

__END_SYS
