#ifndef __ip_layer_h
#define __ip_layer_h

#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <buffers_handler.h>
#include <machine/nic.h>
#include <network/ethernet.h>
#include <utility/list.h>
#include <system.h>

__BEGIN_SYS

class IP: public NIC<Ethernet>::Observer
{
private:
    /// @brief Buffer received by the NIC
    typedef Ethernet::BufferInfo BufferInfo;
    typedef Ethernet::Protocol Protocol;
    typedef Ethernet::Observed Observed;
    typedef Ethernet::Address Address;

    enum FragmentFlags {
        DATAGRAM = 0b100,
        MID_FRAGMENT = 0b010,
        LAST_FRAGMENT = 0b000,
    };

    // The Header of the fragment, the static data to identify it
    class FragmentHeader
    {
    public:

        FragmentHeader() {}
        FragmentHeader(unsigned short total_length, unsigned short id, unsigned char flags, const unsigned short offset) 
            : _total_length(total_length), _id(id), _flags(flags), _offset(offset) { }

        bool more_fragments() const { return _flags == FragmentFlags::MID_FRAGMENT; }
        bool is_last_datagram() const { return _flags == FragmentFlags::LAST_FRAGMENT; }
        bool is_datagram() const { return _flags == FragmentFlags::DATAGRAM; }

        unsigned int id() { return _id; }
        unsigned int offset() { return _offset; }

        friend Debug & operator<<(Debug & db, const FragmentHeader & f) {
            db << "{size=" << f._total_length 
                << ",id=" << f._id 
                << ",f=" << bin << f._flags 
                << ",is_datagram=" << f.is_datagram() 
                << ",more_frags=" << f.more_fragments() 
                << ",last_fragment=" << f.is_last_datagram() 
                << ",index=" << f._offset << "}";
            
            return db;
        }

    protected:
        unsigned short _total_length; // Total length of the datagram, including the header 
        unsigned short _id; // Identifier of the datagram
        unsigned char _flags :3; // Flags (DF, MF and Reserved)
        unsigned int _offset :13; // Offset of this frame in the datagram
    } __attribute__((packed, may_alias));


    /// @brief Maximum data that can be transfer in each Fragment 
    static const unsigned int FRAGMENT_MTU = NIC<Ethernet>::MTU - sizeof(FragmentHeader);
    typedef unsigned char FragmentData[FRAGMENT_MTU];

    /// @brief The Fragment class that will be created in the Fragmentation process
    class Fragment : public FragmentHeader 
    {
    public:
        unsigned static const int DATA_SIZE = sizeof(FragmentData);
        unsigned static const int FRAGMENT_SIZE =  DATA_SIZE + sizeof(FragmentHeader);

        Fragment(unsigned short total_length, unsigned short id, unsigned char flags, const unsigned short offset) 
            : FragmentHeader(total_length, id, flags, offset) { }

        Fragment(unsigned short total_length, unsigned short id, unsigned char flags, const unsigned short offset, 
        const void * data, unsigned int size) : FragmentHeader(total_length, id, flags, offset) 
        {
            memcpy(_data, data, size);
        }

        void * data() { return _data; }

        FragmentHeader * header() { return this; }

        friend Debug & operator<<(Debug & db, const Fragment & f) {
            db << "{h=" << reinterpret_cast<const FragmentHeader &>(f) << ",d=" << f._data << "}";

            return db;
        }

    protected:
        FragmentData _data;
    } __attribute__((packed, may_alias));

    class Header 
    {
        protected:
            typedef Ethernet::Address Address;
            typedef Ethernet::Protocol Protocol;

        public: 
            static const unsigned int SERVICE_TYPE = 0;
            static const unsigned int VERSION = 4;
            static const unsigned int HEADER_LENGTH = 5;
            static const unsigned int TIME_TO_LIVE = 10;

            Header(const Address & src, const Address & dst, const Protocol & prot, unsigned int size, 
            unsigned int id, const FragmentFlags & flags) 
            : _ihl(HEADER_LENGTH), _version(VERSION),  _service_type(SERVICE_TYPE), _length(size), _id(id), _flags(flags), 
            _offset(0), _ttl(TIME_TO_LIVE), _protocol(prot),  _checksum(0), _src(src), _dst(dst)
            { }

            friend Debug & operator<<(Debug & db, const Header & h) {
                db << "{ihl=" << h._ihl
                << ",ver=" << h._version
                << ",tos=" << h._service_type
                << ",len=" << h._length
                << ",id="  << h._id
                << ",flg=" << h._flags
                << ",off=" << h._offset
                << ",ttl=" << h._ttl
                << ",pro=" << h._protocol
                << ",chk=" << h._checksum
                << ",from=" << h._src
                << ",to=" << h._dst
                << "}";

                return db;
            }

        protected: 
            unsigned char _ihl:4; /// IP Header Length
            unsigned char _version:4; /// IP Version
            unsigned char _service_type:8; /// Type of Service
            unsigned short _length:16;     /// Total length of datagram, header + data
            unsigned short _id:16;     /// Datagram ID
            unsigned short _flags:3; /// Flags (Reserved, DF, MF)
            unsigned short _offset:13;     /// Fragment offset
            unsigned char _ttl:8; /// Time To Live
            Protocol _protocol; /// Payload Protocol (RFC 1700)
            unsigned short _checksum:16; /// Header checksum (RFC 1071)
            Address _src; /// Source IP address
            Address _dst; /// Destination IP address

    } __attribute__((packed, may_alias));


public:
    class Datagram : public Header 
    {
        public:
            Datagram(const Address & src, const Address & dst, const Protocol & prot, unsigned int size, unsigned int id, const FragmentFlags & flags, 
            const void * data, unsigned int data_size) : Header(src, dst, prot, size, id, flags)
            { 
                memcpy(_data, data, data_size);
            }

            Header * header() { return this; }

            void * data() { return _data; }
            unsigned int data_size() { return _length; }

            friend Debug & operator<<(Debug & db, const Datagram & f) {
                db << "{datagram=" << reinterpret_cast<const Header &>(f) << "}";

                return db;
            }

        protected:
            /// @brief Placeholder for the data stored in the Datagram
            unsigned char _data[1];
    };

    /// @brief Maximum data stored in a Datagram
    static const unsigned int MTU = 65535 - sizeof(Header);

    /// @brief Maximum data that can be send without fragmentation in one send
    static const unsigned int MTU_WO_FRAG = NIC<Ethernet>::MTU - sizeof(FragmentHeader) - sizeof(Header);
protected:
    IP(NIC<Ethernet> * nic);

public:
    static IP* init(NIC<Ethernet> * nic);

    static const unsigned int IP_PROT = Ethernet::PROTO_IP;

    void send(Address dst, const void * data, unsigned int size);

    void update(Observed * observed, const Protocol & c, BufferInfo * d) override;

    void handle_fragmentation(Fragment * fragment);

    void handle_datagram(Datagram * datagram);

    SiFiveU_NIC * nic() { return _nic; }

private:
    static unsigned int _datagram_count;
    static IP * _ip;
    SiFiveU_NIC * _nic;

    void send_with_fragmentation(Address dst, const void * data, unsigned int size);
    void send_without_fragmentation(Address dst, const void * data, unsigned int size);

    static unsigned int get_next_id();
};

__END_SYS

#endif
