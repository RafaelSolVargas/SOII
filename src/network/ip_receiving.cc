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

    // If this host is not a gateway and is not the destination, then discard the fragment
    if (fragment->destiny() != _address && !_router->is_gateway()) 
    {
        db<IP>(TRC) << "IP::Ignoring datagram that is not for me!" << endl;
    
        return;
    }

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
    if (datagram->verify_checksum()) 
    {
        db<IP>(TRC) << "IP::DatagramWellFormed" << endl;
    } 
    else
    {
        db<IP>(TRC) << "IP::DatagramWithTransmitErrors" << endl;

        return;
    }

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
    db<IP>(TRC) << "IP::Reassembler::HandlingFragment" << endl;

    // Gets the datagram handling the reassembling, if none the method already starts one
    DatagramReassembling * datagram = get_reassembling_datagram(fragment);

    if (datagram->get_fragment_status(fragment->offset())->received() == true)
    {
        // Fragment is duplicated
        return;
    }

    // Update internal informations of the datagram
    datagram->receive_fragment(fragment);

    // Copy the fragment data into the reassembling datagram


    db<IP>(TRC) << "IP::Reassembler::DatagramReassembling=" << *datagram << endl;

    // If completed, handle the datagram being received
    if (datagram->reassembling_completed()) 
    {
        handle_datagram(datagram->header());
    }
}


IP::DatagramReassembling * IP::get_reassembling_datagram(Header * fragment) 
{
    // Check if there is already an constructed datagram reassembling
    for (DatagramReassembling::Element *el = _reassemblingList.head(); el; el = el->next()) 
    {
        DatagramReassembling* datagram = el->object();

        if (datagram->source() == fragment->source() && datagram->destiny() == fragment->destiny() 
         && datagram->protocol() == fragment->protocol() && datagram->id() == fragment->id()) 
        {
            return datagram;
        }
    }

    db<IP>(TRC) << "IP::Reassembler::Creating an new DatagramReassembling" << endl;

    // If not, build one
    MemAllocationMap * map = nw_buffers.alloc_nc(MAX_DATAGRAM_SIZE);

    Header * header = new (SYSTEM) Header(fragment->source(), fragment->destiny(), fragment->protocol(), 0, fragment->id(), FragmentFlags::DATAGRAM, 0);

    DatagramReassembling * datagram = new (SYSTEM) DatagramReassembling(header, map);

    // Insert into list
    _reassemblingList.insert(datagram->link());

    return datagram;
}

__END_SYS
