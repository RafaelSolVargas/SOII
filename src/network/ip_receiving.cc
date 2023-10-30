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
