#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>
#include <memory.h>
#include <machine.h>
#include <time.h>

__BEGIN_SYS


SiFiveU_NIC::SiFiveU_NIC()
{
    _configuration.unit = 1;
    _configuration.timer_frequency = TSC::frequency();
    _configuration.timer_accuracy = TSC::accuracy() / 1000;
    if (!_configuration.timer_accuracy) 
    {
        _configuration.timer_accuracy = 1;
    }

    // Initialize the controller
    GEM::apply_and_mask(R_NW_CTRL, 0);
    GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_CLR_STATS_REGS);
    GEM::apply_or_mask(R_RECEIVE_STATS, 0x0000000F); 
    GEM::apply_or_mask(R_TRANSMIT_STATS, 0x000000FF);
    GEM::apply_or_mask(R_INT_DISABLE, 0x7FF0FEFF);
    GEM::write_value(R_RECEIVE_Q_PTR, 0);
    GEM::write_value(R_TRANSMIT_Q_PTR, 0);

    // Inicializa o DMA_Buffer que irá conter os descritores, faz a criação de um CBuffer
    _rings_buffer = new (SYSTEM) DMA_Buffer(DESC_BUFFER_SIZE);

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::_rings_buffer::address => " << _rings_buffer->phy_address() << endl;

    // Inicializa os rings dos descritores
    void * rx_ring_addr = (_rings_buffer->phy_address());
    void * tx_ring_addr = (_rings_buffer->phy_address() + RX_DESC_BUFFER_SIZE);

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::rx_ring_addr::address => " << rx_ring_addr << endl;
    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::tx_ring_addr::address => " << tx_ring_addr << endl;

    // Inicializa os Rx Descriptors
    _rx_cur = 0;
    _rx_ring_phy = _rings_buffer->phy_address();
    _rx_ring = reinterpret_cast<Rx_Desc *>(rx_ring_addr);
    for (unsigned int i = 0; i < RX_BUFS; i++) 
    {
        new (&_rx_ring[i]) Rx_Desc(); 

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::RX_DESC[" << i << "] => " << &_rx_ring[i] << endl;
    }

    // Inicializa os Tx Descriptors
    _tx_cur = 0;
    _tx_ring_phy = _rings_buffer->phy_address() + RX_DESC_BUFFER_SIZE;
    _tx_ring = reinterpret_cast<Tx_Desc *>(tx_ring_addr);
    for (unsigned int i = 0; i < TX_BUFS; i++) 
    {
        new (&_tx_ring[i]) Tx_Desc();

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::TX_DESC[" << i << "] => " << &_tx_ring[i] << endl;
    }

    // Create the RX Ring Buffers 
    for (unsigned int i = 0; i < RX_BUFS; i++) 
    {
        // A classe CBuffer já utiliza diretamente a classe DMA_Buffer para alocações contíguas 
        _rx_buffers[i] = new (SYSTEM) Buffer(BUFF_SIZE);

        _rx_ring[i].phy_addr = Phy_Addr(_rx_buffers[i]->address()); // Verify how to keep bits [1-0] 
        _rx_ring[i].phy_addr &= ~Rx_Desc::OWNER; // Disable OWNER bit
        _rx_ring[i].phy_addr &= ~Rx_Desc::WRAP; // Disable WRAP bit
        _rx_ring[i].ctrl = 0;

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::RX_BUFF[" << i << "] => " << _rx_buffers[i] << "(Addr=" << _rx_buffers[i]->address() << ")" << endl;
    }
    _rx_ring[RX_BUFS - 1].phy_addr |= Rx_Desc::WRAP; // Set as the last buffer

    // Create the TX Ring Buffers 
    for (unsigned int i = 0; i < TX_BUFS; i++) 
    {
        // A classe CBuffer já utiliza diretamente a classe DMA_Buffer para alocações contíguas 
        _tx_buffers[i] = new (SYSTEM) Buffer(BUFF_SIZE);

        _tx_ring[i].phy_addr = Phy_Addr(_tx_buffers[i]->address()); // Verify how to keep bits [1-0] 
        _tx_ring[i].ctrl |= Tx_Desc::USED; // Write 1, if 0 the DMA will start
        _tx_ring[i].ctrl &= ~Tx_Desc::LAST; // Set as not the last buffer

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::TX_BUFF[" << i << "] => " << _tx_buffers[i] << "(Addr= " << _tx_buffers[i]->address() << ")" << endl;
    }
    _tx_ring[TX_BUFS - 1].ctrl |= Tx_Desc::LAST; // Set as the last buffer

    configure();
}

SiFiveU_NIC::~SiFiveU_NIC() {
    delete _rings_buffer;

    for (unsigned int i = 0; i < RX_BUFS; i++) {
        delete _rx_buffers[i];
    }

    for (unsigned int i = 0; i < TX_BUFS; i++) {
        delete _tx_buffers[i];
    }
}


void SiFiveU_NIC::configure() 
{
    db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::configure()" << endl;
    
    GEM::apply_or_mask(R_NW_CFG, (R_NW_CFG_B_FULL_DUPLEX | R_NW_CFG_B_REMOVE_FCS));
    GEM::apply_and_mask(R_NW_CFG, ~R_NW_CFG_B_PROMISC);

    // Set the MAC address
    for (int i = 0; i < 4; i++)
    {
        Reg32 low = *GEM::reg(R_SPADDR1L + i * 8);
        Reg32 high = *GEM::reg(R_SPADDR1H + i * 8) & 0xFFFF;

        if (low != 0 || high != 0)
        {
            _configuration.address[0] = low & 0xFF;
            _configuration.address[1] = (low >> 8) & 0xFF;
            _configuration.address[2] = (low >> 16) & 0xFF;
            _configuration.address[3] = (low >> 24) & 0xFF;
            _configuration.address[4] = high & 0xFF;
            _configuration.address[5] = (high >> 8) & 0xFF;
        
            break;
        };
    };

    *GEM::reg(R_SPADDR1L) = (_configuration.address[3] << 24) |
                         (_configuration.address[2] << 16) |
                         (_configuration.address[1] << 8) | 
                         _configuration.address[0];
    
    *GEM::reg(R_SPADDR1H) = (_configuration.address[5] << 8) | 
                          _configuration.address[4];

    for (int i = 1; i < 4; i++)
    {
        *GEM::reg(R_SPADDR1L + i * 8) = 0;
        *GEM::reg(R_SPADDR1H + i * 8) = 0;
    };

    // Set up DMA control register
    // *GEM::reg(R_DMA_CFG) = dma_cfg_rx_size(sizeof(Frame) + sizeof(Header));

    // Set up the physical address of Rx and Tx rings buffers
    *GEM::reg(R_RECEIVE_Q_PTR) = _tx_ring_phy;
    *GEM::reg(R_TRANSMIT_Q_PTR) = _rx_ring_phy;

    // Enable tx and rx
    GEM::apply_or_mask(R_NW_CTRL, (R_NW_CTRL_B_RX_EN | R_NW_CTRL_B_TX_EN));

    // Enable interrupts
    // Only those supported from QEMU (real hardware might have more)
    // Except TX_COMPLETE, for better performance
    *GEM::reg(R_INT_ENABLE) |= R_INT_ENABLE_B_RX_COMPLETE 
                            | R_INT_ENABLE_B_TX_CORRUPT_AHB_ERR 
                            | R_INT_ENABLE_B_TX_USED_READ 
                            | R_INT_ENABLE_B_RX_USED_READ;

    GEM::print_register(R_NW_CTRL);
    GEM::print_register(R_NW_CFG);
    GEM::print_register(R_DMA_CFG);
    GEM::print_register(R_IMR);
}

SiFiveU_NIC::Buffer * SiFiveU_NIC::alloc(const Address & dst, const Protocol & prot, unsigned int once, unsigned int always, unsigned int payload) 
{
    return _rx_buffers[0];
}

int SiFiveU_NIC::send(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    return 1;
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
