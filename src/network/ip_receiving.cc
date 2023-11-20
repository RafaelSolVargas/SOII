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
    if (_dataCallbacks.empty()) 
    {
        return;
    }

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

void IP::handle_datagram_reassembled(IP::DatagramReassembling * datagram) 
{
    if (datagram->destiny() != _address) 
    {
        if (_router->is_gateway()) 
        {
            db<IP>(TRC) << "IP::RoutingDatagram::DST=(" << datagram->destiny() << ")" << endl;

            // Reuse the map that was used to reassemble the datagram
            MemAllocationMap * map = reinterpret_cast<MemAllocationMap*>(datagram->map());

            SendingParameters parameters = SendingParameters(datagram->destiny(), datagram->protocol(), HIGH);

            // Insert the datagram in the queue to be sended
            DatagramSending* datagram_buffered = new (SYSTEM) DatagramSending(parameters, map, *datagram->header()); 

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

    db<IP>(TRC) << "IP::Datagram Received and Reassembled=" << endl;

    execute_callbacks(datagram->header(), datagram->map());

    IPEventsHandler::process_event(IPEventsHandler::HOST_UNREACHABLE);
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
            DatagramSending* datagram_buffered = new (SYSTEM) DatagramSending(parameters, map, header_to_reuse); 

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

    // Move the data from the NIC Buffer to the AllocationMap
    unsigned int data_size = datagram->data_size();

    void * buffered_data = nw_buffers.alloc_nc(const_cast<char*>(reinterpret_cast<const char*>(datagram->data_address())), data_size);

    MemAllocationMap * map = reinterpret_cast<MemAllocationMap*>(buffered_data);

    // Pass the received datagrams for upper layers
    execute_callbacks(datagram, map);

    // Increase counter, execute this as last to not finish threads before the callback is executed
    _stats.rx_datagrams++;
}

void IP::execute_callbacks(Header * datagram, MemAllocationMap * map) 
{
    if (_dataCallbacks.empty()) 
    {
        return;
    }

    // Call all callbacks that was registered in the NIC
    for (IpDataCbWrapper::Element *el = _dataCallbacks.head(); el; el = el->next()) 
    {
        IpDataCbWrapper* wrapper = el->object();

        if (wrapper->protocol() == datagram->protocol()) 
        {
            db<SiFiveU_NIC>(INF) << "IP::CallbackExecutor::Sending Datagram[" << datagram->id() << "]" << endl;

            wrapper->_callback(datagram, map);

            db<SiFiveU_NIC>(INF) << "IP::CallbackExecutor::Finished Datagram [" << datagram->id() << "]" << endl;
        }
    }

    nw_buffers.free_nc(map);

    delete datagram;
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
    nw_buffers.copy_to_buffer(datagram->map(), fragment->data_address(), fragment->data_size(), fragment->data_offset());

    // If completed, handle the datagram being received
    if (datagram->reassembling_completed()) 
    {
        db<IP>(TRC) << "IP::Reassembler::DatagramReassembled=" << *datagram << endl;

        handle_datagram_reassembled(datagram);
    }
}

IP::DatagramReassembling * IP::get_reassembling_datagram(Header * fragment) 
{
    remove_expired_datagrams();
    
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

void IP::remove_expired_datagrams() 
{
    // Check if there is an expired timestamp
    for (DatagramReassembling::Element *el = _reassemblingList.head(); el; el = el->next()) 
    {
        DatagramReassembling* datagram = el->object();

        if (datagram->is_expired()) 
        {
            db<IP>(TRC) << "IP::Reassembler::DatagramExpired!!!!" << endl;
        
            // Tells the Events handler that an datagram has expired
            IPEventsHandler::process_event(IPEventsHandler::REASSEMBLING_TIMEOUT);
        
            _reassemblingList.remove(datagram->link());

            // Libera os buffers
            nw_buffers.free_nc(datagram->map());

            // Deleta o objeto
            delete datagram;
        }
    }
}

__END_SYS
