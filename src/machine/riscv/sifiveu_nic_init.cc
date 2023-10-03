#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>
#include <memory.h>
#include <machine.h>
#include <time.h>

__BEGIN_SYS

SiFiveU_NIC * SiFiveU_NIC::_device;

/// @brief Method to get an instance of the SiFiveU_NIC
SiFiveU_NIC* SiFiveU_NIC::init() 
{
    if (!_device) {
        _device = new (SYSTEM) SiFiveU_NIC();
    } 

    return _device;
}

SiFiveU_NIC::SiFiveU_NIC()
{
    SiFiveU_NIC::_device = this;

    _configuration.unit = 1;
    _configuration.timer_frequency = TSC::frequency();
    _configuration.timer_accuracy = TSC::accuracy() / 1000;
    if (!_configuration.timer_accuracy) 
    {
        _configuration.timer_accuracy = 1;
    }

    // Initialize the controller
    GEM::write_value(R_NW_CTRL, 0);
    GEM::write_value(R_NW_CTRL, R_NW_CTRL_B_CLR_STATS_REGS);
    
    GEM::apply_and_mask(R_NW_CFG, R_NW_CFG_32_WIDTH_SIZE);

    GEM::write_value(R_RECEIVE_STATS, 0x0000000F); 
    GEM::write_value(R_TRANSMIT_STATS, 0x000001FF);
    
    GEM::write_value(R_RECEIVE_Q_PTR, 0);
    GEM::write_value(R_TRANSMIT_Q_PTR, 0);

    GEM::write_value(R_INT_DISABLE, R_IDR_INT_ALL);
    
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
    _tx_last_unlocked = -1;
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
        _rx_ring[i].phy_addr &= ~Rx_Desc::OWNER; // Write 0, now the GEM controls this buffer
        _rx_ring[i].phy_addr &= ~Rx_Desc::WRAP; // Disable WRAP bit
        _rx_ring[i].ctrl = 0;

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::RX_DESC[" << i << "] => " << _rx_ring[i] << endl;
        db<SiFiveU_NIC>(INF) << "Buffer => " << _rx_buffers[i] << "(Addr=" << _rx_buffers[i]->address() << ")\n" << endl;
    }
    _rx_ring[RX_BUFS - 1].phy_addr |= Rx_Desc::WRAP; // Set as the last buffer

    // Create the TX Ring Buffers 
    for (unsigned int i = 0; i < TX_BUFS; i++) 
    {
        // A classe CBuffer já utiliza diretamente a classe DMA_Buffer para alocações contíguas 
        _tx_buffers[i] = new (SYSTEM) Buffer(BUFF_SIZE);

        _tx_ring[i].phy_addr = Phy_Addr(_tx_buffers[i]->address());
        _tx_ring[i].ctrl |= Tx_Desc::OWNER; // Write 1, is owned by NIC
        _tx_ring[i].ctrl &= ~Tx_Desc::WRAP; // Set as not the last buffer

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::TX_DESC[" << i << "] => " << _tx_ring[i] << endl;
        db<SiFiveU_NIC>(INF) << "Buffer => " << _tx_buffers[i] << "(Addr=" << _tx_buffers[i]->address() << ")\n" << endl;
    }
    _tx_ring[TX_BUFS - 1].ctrl |= Tx_Desc::WRAP; // Set as the last buffer

    configure();
}

void SiFiveU_NIC::configure() 
{
    db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::configure()" << endl;
    
    // Enable Full Duplex and FCS
    GEM::apply_or_mask(R_NW_CFG, R_NW_CFG_B_FULL_DUPLEX);

    // If configured to not bring the FCS bits to memory configure it in the GEM
    if (!BRING_FCS_TO_MEM) {
        GEM::apply_or_mask(R_NW_CFG, R_NW_CFG_B_REM_FCS);
    }

    // Disable Promiscuous
    GEM::apply_and_mask(R_NW_CFG, ~R_NW_CFG_B_PROMISC);

    // Configure MAC Address
    configure_mac();

    // Configure the interruptions
    configure_int();

    // Configure the DMA control register
    *GEM::reg(R_DMA_CFG) = ((sizeof(Frame) + sizeof(Header)) / 64) << 16;

    // Configure the physical address of Rx ring buffer
    *GEM::reg(R_RECEIVE_Q_PTR) = _rx_ring_phy;

    // Configure the physical address of Tx ring buffer
    *GEM::reg(R_TRANSMIT_Q_PTR) = _tx_ring_phy;

    // Enable tx and rx
    GEM::apply_or_mask(R_NW_CTRL, (R_NW_CTRL_B_RX_EN | R_NW_CTRL_B_TX_EN));

    // Enable interrupts
    *GEM::reg(R_INT_ENABLE) |= R_INT_ENABLE_B_RX_COMPLETE 
                            | R_INT_ENABLE_B_TX_CORRUPT_AHB_ERR 
                            | R_INT_ENABLE_B_TX_USED_READ 
                            | R_INT_ENABLE_B_RX_USED_READ;

    db<Heaps, System>(INF) 
        << "SiFiveU_NIC::configure(): R_NW_CFG=" << hex << GEM::reg_value(R_NW_CFG) << " \n"
        << "SiFiveU_NIC::configure(): R_NW_CTRL=" << hex << GEM::reg_value(R_NW_CTRL) << "\n"
        << "SiFiveU_NIC::configure(): R_TRANSMIT_Q_PTR=" << hex << GEM::reg_value(R_TRANSMIT_Q_PTR) << "\n"
        << "SiFiveU_NIC::configure(): R_RECEIVE_Q_PTR=" << hex << GEM::reg_value(R_RECEIVE_Q_PTR) << "\n"
        << "SiFiveU_NIC::configure(): R_IDR=" << hex << GEM::reg_value(R_INT_DISABLE) << "\n"
        << "SiFiveU_NIC::configure(): R_TRANSMIT_STATS=" << hex << GEM::reg_value(R_TRANSMIT_STATS) << "\n"
        << "SiFiveU_NIC::configure(): R_RECEIVE_STATS=" << hex << GEM::reg_value(R_RECEIVE_STATS) << "\n"
        << "SiFiveU_NIC::configure(): R_DMA_CFG=" << hex << (GEM::reg_value(R_DMA_CFG) & (0xff << 16)) << "  \n"
        << "SiFiveU_NIC::configure(): R_IMR=" << hex << GEM::reg_value(R_IMR) << endl;
}

void SiFiveU_NIC::configure_mac() 
{
    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::configure_mac()" << endl;

    // Set the MAC address
    // https://github.com/freebsd/freebsd-src/blob/main/sys/dev/cadence/if_cgem.c
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


    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::MAC => " << address() << endl;
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

void SiFiveU_NIC::configure_int() 
{
    // Save this instance to be called with the handler afterwards
    SiFiveU_NIC::_device = this;

    // Install interruption_handler method as the handler
    IC::int_vector(IC::INT_GIGABIT_ETH, &interruption_handler);

    // Enable interrupts
    IC::enable(IC::INT_GIGABIT_ETH);

    // Change priority of interrupts
    IC::set_external_priority(IC::INT_GIGABIT_ETH, 7);
}

__END_SYS
