// EPOS Network Interface Mediator Common Package

#ifndef __si_five_u_nic_h
#define __si_five_u_nic_h

#include <utility/nic_buffers.h>
#include <architecture/cpu.h>
#include <architecture/tsc.h>
#include <architecture/mmu.h>
#include <network/ethernet.h>

__BEGIN_SYS

class GEM
{
protected:
    typedef CPU::Reg8 Reg8;
    typedef CPU::Reg16 Reg16;
    typedef CPU::Reg32 Reg32;
    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef MMU::DMA_Buffer DMA_Buffer;
    typedef Ethernet::Address MAC_Address;

public:
    /* Cadence GEM hardware register definitions. */
    #define ETH_BASE 0x10090000	

    /*Network Control Register */
    #define R_NW_CTRL 0x0000
    #define R_NW_CTRL_B_CLR_STATS_REGS (1 << 5)
    #define R_NW_CTRL_B_TX_EN (1 << 3)   /*< Transmit enable */
    #define R_NW_CTRL_B_RX_EN (1 << 2)   /*< Receive enable */
    #define R_NW_CTRL_B_TX_START (1 << 9) /*< Transmit start */

    /* Network Config Register */
    #define R_NW_CFG 0x0004
    #define R_NW_CFG_B_FULL_DUPLEX (1 << 1)
    #define R_NW_CFG_B_FCS (1 << 17)
    #define R_NW_CFG_B_PROMISC (1 << 4)

    #define R_NET_STATS 0x0008
    #define R_DMA_CFG 0x010
    #define R_TRANSMIT_STATS 0x0014
    #define R_RECEIVE_Q_PTR 0x0018
    #define R_TRANSMIT_Q_PTR 0x001C
    #define R_RECEIVE_STATS 0x0020
    #define R_INT_DISABLE 0x002C
    #define R_IMR 0x030         /*< Interrupt Mask reg */

    /*< Interrupt Enable reg */
    #define R_INT_ENABLE 0x028  
    #define R_INT_ENABLE_B_TX_CORRUPT_AHB_ERR 1 << 6
    #define R_INT_ENABLE_B_TX_USED_READ 1 << 3
    #define R_INT_ENABLE_B_RX_USED_READ 1 << 2
    #define R_INT_ENABLE_B_RX_COMPLETE 1 << 1

    /* Registers */
    #define R_SPADDR1L 0x088 /*< Specific1 addr low reg */
    #define R_SPADDR1H 0x08C /*< Specific1 addr high reg */


    // Transmit and Receive Descriptors (in the Ring Buffers)
    // GEM Descriptors only have two words
    struct Desc {
        volatile Reg32 phy_addr;
        volatile Reg32 ctrl;
    };

    // Receive Descriptor
    struct Rx_Desc: public Desc {
        enum {
            // Others bits is for address
            WRAP = 1,
            OWNER = 0,
            SIZE_MASK = 0x3fff
        };

        // Aplica máscara e altera o valor de size do descriptor
        void update_size(unsigned int size) {
            ctrl = (ctrl & ~SIZE_MASK) | (size & SIZE_MASK);
        }

        friend Debug &operator<<(Debug &db, const Rx_Desc &d) 
        {
            db << "{" << hex << d.phy_addr << dec << ","
                << (d.ctrl & SIZE_MASK) << "," << hex << d.ctrl << dec << "}";
            return db;
        }
    };

    // Transmit Descriptor
    struct Tx_Desc: public Desc {
        enum {
            OWNER = 31,
            LAST = 15,
            SIZE_MASK = 0x1fff
            // 14 is reserved
            // 13-1 is for length of buffer
        };

        // Aplica máscara e altera o valor de size do descriptor
        void update_size(unsigned int size) {
            ctrl = (ctrl & ~SIZE_MASK) | (size & SIZE_MASK);
        }

        friend Debug &operator<<(Debug &db, const Tx_Desc &d) 
        {
            db << "{" << hex << d.phy_addr << dec << ","
                << (d.ctrl & SIZE_MASK) << "," << hex << d.ctrl << dec << "}";
            return db;
        }
    };

    static volatile Reg32 * reg(unsigned int offset) {
        return reinterpret_cast<volatile Reg32*>(ETH_BASE + offset);
    }

    static void write_value(int offset, Reg32 value) 
    { 
        *reinterpret_cast<Reg32*>(ETH_BASE + offset) = value;
    }

    static void apply_and_mask(int offset, Reg32 mask) {
        Reg32* addr = reinterpret_cast<Reg32*>(ETH_BASE + offset);
        *addr = *addr & mask;
    }

    static void apply_or_mask(int offset, Reg32 mask) {
        Reg32* addr = reinterpret_cast<Reg32*>(ETH_BASE + offset);
        *addr = *addr | mask;
    }

    static bool read_bit(int registerOffset, int bitIndex) {
        Reg32 regValue = *reg(registerOffset);

        // Crie uma máscara com um único bit definido na posição desejada
        Reg32 mask = 1U << bitIndex;

        // Aplique a máscara e desloque o bit para a posição 0
        Reg32 result = (regValue & mask) >> bitIndex;

        return result != 0;
    }

    static void print_register(int registerOffset) 
    {
        db<SiFiveU_NIC>(INF) << "Register Offset(" << registerOffset << ") = ";
        for (int x = 31; x >= 0; x--) 
        {
            bool bitValue = GEM::read_bit(registerOffset, x);

            db<SiFiveU_NIC>(INF) << bitValue;
        }

        db<SiFiveU_NIC>(INF) << endl;
    }

    /* static void write_bit_range(int registerOffset, int firstBit, int quant, bool bitValue) 
    {
        for (int bitOffset = firstBit; bitOffset <= quant; bitOffset++) 
        {
            write_bit(registerOffset, bitOffset, bitValue);
        }
    } */

    /* static void write_bit(int registerOffset, int bitIndex, bool bitValue) {
        Reg32 regValue = *reg(registerOffset);

        Reg32 mask = static_cast<Reg32>(1U << bitIndex);

        if (bitValue) 
        {
            regValue |= mask;
        } 
        else 
        {
            regValue &= ~mask;
        }

        write_value(registerOffset, regValue);
    } */

    /* void set_value(int offset, int value) {
        Reg32 * pointer = reinterpret_cast<Reg32 *>(ETH_BASE + offset);
        
        *pointer = value;
    } */
};


class SiFiveU_NIC: private NIC<Ethernet>, private GEM
{
    friend class Machine_Common;
    friend class Init_System;
    friend class Init_Application;

private:
    // Transmit and Receive Ring sizes
    static const unsigned int UNITS = Traits<SiFiveU_NIC>::UNITS;
    static const unsigned int TX_BUFS = Traits<SiFiveU_NIC>::SEND_BUFFERS;
    static const unsigned int RX_BUFS = Traits<SiFiveU_NIC>::RECEIVE_BUFFERS;
    static const unsigned int BUFF_SIZE = sizeof(Ethernet::Frame) + sizeof(long);

    // Size of the DMA Buffer that will host the only ring buffers
    static const unsigned int RX_DESC_BUFFER_SIZE = RX_BUFS * ((sizeof(Rx_Desc) + 15) & ~15U); 
    static const unsigned int TX_DESC_BUFFER_SIZE = TX_BUFS * ((sizeof(Tx_Desc) + 15) & ~15U); 
    static const unsigned int DESC_BUFFER_SIZE = RX_DESC_BUFFER_SIZE + TX_DESC_BUFFER_SIZE;

public:
    class AllocBufferInfo 
    {
    public:
        typedef Simple_List<AllocBufferInfo> List;
        typedef typename List::Element Element;
        typedef SiFiveU_NIC::Buffer Buffer;

        AllocBufferInfo(Buffer * buffer, unsigned int index, unsigned long size) : _buffer(buffer), _index(index), 
            _size(size),  _link1(this), _link2(this) { }

        SiFiveU_NIC::Buffer * buffer() { return _buffer; }
        unsigned int index() { return _index; }
        unsigned long size() { return _size; }
        Element * link1() { return &_link1; }
        Element * link() { return link1(); }
        Element * lint() { return link1(); }
        Element * link2() { return &_link2; }
        Element * lext() { return link2(); } 

    private: 
        Buffer * _buffer;
        unsigned int _index;
        unsigned long _size;
        Element _link1;
        Element _link2;
    };

    SiFiveU_NIC();
    ~SiFiveU_NIC();

    /// @brief Allocate a Buffer and immediately triggers the send to the GEM
    /// @param dst The destination MAC address
    /// @param prot The protocol being used
    /// @param data The data to be passed
    /// @param size The size of the data
    int send(const Address & dst, const Protocol & prot, const void * data, unsigned int size);
    
    /// @brief 
    /// @param src 
    /// @param prot 
    /// @param data 
    /// @param size 
    int receive(Address * src, Protocol * prot, void * data, unsigned int size);

    /// @brief Allocate one or more Buffers in a Simple_List<AllocBufferInfo> to allow the user to copy his data
    /// into the buffers, the list returned will be passed to the send(AllocBufferInfo*) afterwards 
    /// @param dst The destination MAC address
    /// @param prot The protocol being used to send the data
    /// @param payload The size of data that need to be passed
    AllocBufferInfo * alloc(const Address & dst, const Protocol & prot, unsigned int payload);

    /// @brief Receive a AllocBufferInfo * that could be a list of pre allocated Buffers and execute the sending of them
    /// @param allocated_buffer The returned AllocBufferInfo * from the AllocBufferInfo * alloc() method
    int send(AllocBufferInfo * allocated_buffer);
    
    /// @brief 
    /// @param buf 
    bool drop(AllocBufferInfo * buf);
    
    /// @brief 
    /// @param buf 
    void free(AllocBufferInfo * buf);

    bool reconfigure(const Configuration * c = 0); // pass null to reset
    void address(const Address &);
    const Address & address() { return _address; }
    const Configuration & configuration() { return _configuration; }
    const Statistics & statistics() { return _statistics; }

    /// @brief DO NOT USE THIS METHOD
    Buffer * alloc(const Address & dst, const Protocol & prot, unsigned int once, unsigned int always, unsigned int payload) { return _rx_buffers[0]; }
    /// @brief DO NOT USE THIS METHOD
    int send(Buffer * buf) { return 1; }
    /// @brief DO NOT USE THIS METHOD
    bool drop(Buffer * buf) { return true; } // after send, while still in the working queues, not supported by many NICs
    /// @brief DO NOT USE THIS METHOD
    void free(Buffer * buf) { } // to be called by observers after handling notifications from the NIC

private:
    // Interrupt dispatching binding
    struct Device {
        PCNet32 * device;
        unsigned int interrupt;
    };

    void configure();
    void configure_mac();
    void receive();

private:
    Configuration _configuration;
    Statistics _statistics;
    Address _address;

    DMA_Buffer* _rings_buffer;

    int _rx_cur;
    Rx_Desc * _rx_ring;
    Phy_Addr _rx_ring_phy;

    int _tx_cur;
    Tx_Desc * _tx_ring;
    Phy_Addr _tx_ring_phy;

    Buffer * _rx_buffers[RX_BUFS];
    Buffer * _tx_buffers[TX_BUFS];

    static Device _devices[UNITS];
};


__END_SYS

#endif