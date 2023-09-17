#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>
#include <memory.h>
#include <machine.h>
#include <time.h>

extern "C" { void _panic(); }

__BEGIN_SYS

SiFiveU_NIC::Buffer * SiFiveU_NIC::alloc(const Address & dst, const Protocol & prot, unsigned int once, unsigned int always, unsigned int payload) 
{
    return _rx_buffers[0];
}

int SiFiveU_NIC::send(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::send(s=" << _configuration.address
                        << ",d=" << dst << ",p=" << hex << prot << dec
                        << ",d=" << data << ",s=" << size << ")" << endl;

    // Busy waiting até conseguir pegar o próximo buffer para fazer o envio 
    unsigned long i = _tx_cur;
    for (bool locked = false; !locked;)
    {
        for (; !(_tx_ring[i].ctrl & Tx_Desc::OWNER); ++i %= TX_BUFS);

        locked = _tx_buffers[i]->lock();
    }

    // Atualiza o próximo tx_current    
    _tx_cur = (i + 1) % TX_BUFS; 

    Tx_Desc* descriptor = &_tx_ring[i];
    Buffer* buffer = _tx_buffers[i];

    // Calculate the size of buffer required
    unsigned long total_size = size + sizeof(Header);

    // Verify the total_size is more than what is available in the CBuffer
    if (total_size > BUFF_SIZE) 
    {
        db<System>(ERR) << "SiFiveU_NIC::send(this=" << this << "): Tried to send frame with " 
                            << total_size << " bytes. Maximum size is: " << BUFF_SIZE << " bytes!" << endl;

        _panic();
    }

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:Buffer[" << i << "]=" << buffer << " => " << *buffer << endl;

    // Store the Frame in the CBuffer
    new (buffer->alloc(total_size)) Frame(_configuration.address, dst, prot, data, size);

    // Update descriptor
    descriptor->ctrl &= ~Tx_Desc::OWNER; 
    descriptor->update_size(total_size);
    descriptor->ctrl |= Tx_Desc::LAST;

    // Start the DMA
    GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_TX_START);

    _statistics.tx_packets++;
    _statistics.tx_bytes += size;

    _tx_buffers[i]->unlock();

    return size;
}

int SiFiveU_NIC::send(Buffer * buf) 
{
    return 1;
}

int SiFiveU_NIC::receive(Address * src, Protocol * prot, void * data, unsigned int size) 
{
    return 1;
}

// after send, while still in the working queues, not supported by many NICs
bool SiFiveU_NIC::drop(Buffer * buf) 
{ 
    return false; 
} 

// to be called by observers after handling notifications from the NIC
void SiFiveU_NIC::free(Buffer * buf) 
{

}

const SiFiveU_NIC::Address & SiFiveU_NIC::address() 
{
    return _address;
}

void SiFiveU_NIC::address(const Address &) 
{

}

// pass null to reset
bool SiFiveU_NIC::reconfigure(const Configuration * c) 
{
    return true;
} 

const SiFiveU_NIC::Configuration & SiFiveU_NIC::configuration() 
{
    return _configuration;
}

const SiFiveU_NIC::Statistics & SiFiveU_NIC::statistics() 
{
    return _statistics;
}

__END_SYS
