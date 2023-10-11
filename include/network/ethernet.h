// EPOS Ethernet Mediator Common Package

#ifndef __ethernet_h
#define __ethernet_h

#include <architecture/cpu.h>
#define __nic_common_only__
#include <machine/nic.h>
#undef __nic_common_only__
#include <utility/list.h>
#include <utility/observer.h>
#include <utility/buffer.h>
#include <utility/nic_buffers.h>

__BEGIN_SYS

class Ethernet: public NIC_Common
{
protected:
    static const unsigned int HEADER_SIZE = 14;

public:
    typedef NIC_Common::Address<6> Address;

    typedef unsigned short Protocol;
    enum {
        PROTO_IP     = 0x0800,
        PROTO_ARP    = 0x0806,
        PROTO_RARP   = 0x8035,
        PROTO_TSTP   = 0x8401,
        PROTO_ELP    = 0x8402,
        PROTO_PTP    = 0x88f7
    };

    // The Ethernet Header (RFC 894)
    class Header
    {
    public:
        Header() {}
        Header(const Address & src, const Address & dst, const Protocol & prot): _dst(dst), _src(src), _prot(htons(prot)) {}

        const Address & src() const { return _src; }
        const Address & dst() const { return _dst; }

        Protocol prot() const { return ntohs(_prot); }

        friend Debug & operator<<(Debug & db, const Header & h) {
            db << "{d=" << h._dst << ",s=" << h._src << ",p=" << hex << h.prot() << dec << "}";
            return db;
        }

    protected:
        Address _dst;
        Address _src;
        Protocol _prot;
    } __attribute__((packed, may_alias));

    // Data and Trailer
    static const unsigned int MTU = 1500;
    typedef unsigned char Data[MTU];

    typedef NIC_Common::CRC32 CRC;
    typedef CRC Trailer;

    // The Ethernet Frame (RFC 894)
    class Frame: public Header
    {
    public:
        Frame() {}
        Frame(const Address & src, const Address & dst, const Protocol & prot): Header(src, dst, prot) {}
        Frame(const Address & src, const Address & dst, const Protocol & prot, const void * data, unsigned int size): Header(src, dst, prot) { memcpy(_data, data, size); }

        Header * header() { return this; }

        void * data() { return _data; }

        friend Debug & operator<<(Debug & db, const Frame & f) {
            db << "{h=" << reinterpret_cast<const Header &>(f) << ",d=" << f._data << "}";
            return db;
        }

    protected:
        Data _data;
        CRC _crc;
    } __attribute__((packed));

    typedef Frame PDU;

    typedef IF<Traits<TSTP>::enabled, TSTP_Metadata, Dummy_Metadata>::Result Metadata;

    // Contiguous Buffer
    typedef _UTIL::CBuffer Buffer;

    class BufferInfo 
    {
    public:
        // Only the SiFiveU_NIC class has access to all methods of this class, the public methods
        // allows others Internet Layers to access the data provided by this class 
        friend class SiFiveU_NIC;

        typedef Simple_List<BufferInfo> List;
        typedef typename List::Element Element;
        typedef CPU::Phy_Addr Phy_Addr;

        /// @brief The size of the data stored in this buffer
        unsigned long size() { return _size; }

        void * data() { return _data_address; }

        Element * link1() { return &_link1; }
        Element * link() { return link1(); }
        Element * lint() { return link1(); }
        Element * link2() { return &_link2; }
        Element * lext() { return link2(); } 

    protected:
        BufferInfo(Buffer * buffer, unsigned int index, unsigned long size) : 
            _buffer(buffer), 
            _index(index), 
            _size(size), 
            _original_size(size),
            _data_address(Phy_Addr(buffer->address())), 
            _link1(this),
            _link2(this) { }

    private:
        Buffer * buffer() { return _buffer; }
        unsigned int index() { return _index; }

        void shrink(unsigned long s) { _size -= s; };
        void shrink_left(unsigned long s) { _data_address += s; shrink(s); };

        Buffer * _buffer; // The CBuffer in the buffers ring
        unsigned int _index; // Index of the buffer in the Rings 
        unsigned long _size; // The size to be showed to the higher layers
        unsigned long _original_size; // The original size of this Buffer data in the Physical layer
        Phy_Addr _data_address; // The address to be access by the higher layers

        Element _link1;
        Element _link2;
    };

    // Observers of a protocol get a also a pointer to the received buffer
    typedef Data_Observing<BufferInfo, Protocol> Observer;
    typedef Data_Observed<BufferInfo, Protocol> Observed;

    // Ethernet NICs usually don't export the timer for SFD time stamping, so the basic time type is set to TSC
    // NICs that do feature time stamping must have any different type converted to TSC::Time_Stamp
    typedef TSC::Time_Stamp Time_Stamp;

    // Configuration parameters
    struct Configuration: public NIC_Common::Configuration
    {
        friend Debug & operator<<(Debug & db, const Configuration & c) {
            db << "{unit=" << c.unit
               << ",addr=" << c.address
               << ",a=" << c.timer_accuracy
               << ",f=" << c.timer_frequency
               << "}";
            return db;
        }

        unsigned int unit;
        Address address;
        PPM timer_accuracy;
        Hertz timer_frequency;
    };

    // Meaningful statistics for Ethernet
    struct Statistics: public NIC_Common::Statistics
    {
        Statistics(): rx_overruns(0), tx_overruns(0), frame_errors(0), carrier_errors(0), collisions(0) {}

        friend Debug & operator<<(Debug & db, const Statistics & s) {
            db << "{rxp=" << s.rx_packets
               << ",rxb=" <<  s.rx_bytes
               << ",rxorun=" <<  s.rx_overruns
               << ",txp=" <<  s.tx_packets
               << ",txb=" <<  s.tx_bytes
               << ",txorun=" <<  s.tx_overruns
               << ",frm=" <<  s.frame_errors
               << ",car=" <<  s.carrier_errors
               << ",col=" <<  s.collisions
               << "}";
            return db;
        }

        Time_Stamp time_stamp;
        Count rx_overruns;
        Count tx_overruns;
        Count frame_errors;
        Count carrier_errors;
        Count collisions;
    };

protected:
    Ethernet() {}

public:
    static const unsigned int mtu() { return MTU; }
    static const Address broadcast() { return Address::BROADCAST; }
};

__END_SYS

#endif
