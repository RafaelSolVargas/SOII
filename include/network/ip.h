#ifndef __ip_layer_h
#define __ip_layer_h

#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <buffers_handler.h>
#include <machine/nic.h>
#include <network/ethernet.h>
#include <utility/list.h>
#include <system.h>
#include <utility/queue.h>

__BEGIN_SYS


class IPDefinitions
{
protected:
    /// @brief Buffer received by the NIC
    typedef Ethernet::BufferInfo BufferInfo;
    typedef Ethernet::Observed Observed;

    typedef Ethernet::Address MAC_Address;
    typedef NIC_Common::Address<4> Address;

    // IP Protocol for NIC
    static const unsigned int PROTOCOL = Ethernet::PROTO_IP;

public:
    // RFC 1700 Protocols
    typedef unsigned char Protocol;
    enum {
        ICMP    = 0x01,
        TCP     = 0x06,
        UDP     = 0x11,
        RDP     = 0x1b,
        TTP     = 0x54
    };

    enum FragmentFlags {
        DATAGRAM = 0b100,
        MID_FRAGMENT = 0b010,
        LAST_FRAGMENT = 0b000,
    };

protected:
    typedef NonCBuffer::AllocationMap AllocationMap;

    // Priority for sending datagrams, will use the Criterion defined for the Threads
    typedef Traits<Thread>::Criterion Priority;
    enum {
        HIGH    = Priority::HIGH,
        NORMAL  = Priority::NORMAL,
        LOW     = Priority::LOW,
    };

    // IP Datagram Configuration
    struct Configuration 
    {
        Configuration(const Address & dst, const Protocol & prot, const Priority & p = Priority::NORMAL) : address(dst), protocol(prot), priority(p) { }

        Address address;
        Protocol protocol;
        Priority priority;
    };

    // Class that will be stored in sending queue
    class DatagramBufferedRX 
    {
    public:
        DatagramBufferedRX(const Configuration & config, MemAllocationMap * map) : _link(this), _configuration(config), _allocation_map(map) { }

        typedef typename Queue<DatagramBufferedRX>::Element Element;
    
        typedef Priority Criterion;

        typedef Ordered_Queue<DatagramBufferedRX, Priority, Scheduler<DatagramBufferedRX>::Element> Queue;
        
        Queue::Element * link() { return &_link; }
        Configuration config() const { return _configuration; }
        MemAllocationMap * map() { return _allocation_map; }

    private:
        Queue::Element _link;
        Configuration _configuration;
        MemAllocationMap * _allocation_map;
    };

    typedef DatagramBufferedRX::Queue Queue;

public:
    class Header
    {
        public: 
            static const unsigned int SERVICE_TYPE = 0;
            static const unsigned int VERSION = 4;
            static const unsigned int HEADER_LENGTH = 5; // 5 bytes
            static const unsigned int TIME_TO_LIVE = 10;

            static const unsigned int FRAGMENT_MTU = NIC<Ethernet>::MTU - (HEADER_LENGTH * 4);

            /// @brief Creates an Datagram with only the Header
            /// @param src Source IP Address
            /// @param dst Destination IP Address
            /// @param prot Protocol being used
            /// @param total_size Total size of the datagram
            /// @param id Identifier of datagram
            /// @param flags Flag of the datagram
            /// @param offset If is a fragment, the offset
            Header(const Address & src, const Address & dst, const Protocol & prot, unsigned int total_size, 
            unsigned int id, const FragmentFlags & flags, unsigned short offset) 
            : _ihl(HEADER_LENGTH), _version(VERSION),  _service_type(SERVICE_TYPE), _id(htons(id)), _flags(flags), 
              _offset(offset), _ttl(TIME_TO_LIVE), _protocol(prot), _checksum(0), _src(src), _dst(dst)
            {
                if (_flags == FragmentFlags::LAST_FRAGMENT) 
                {
                    unsigned int last_fragment_data = (total_size % FRAGMENT_MTU);
                    if (last_fragment_data == 0) 
                    {
                        last_fragment_data = FRAGMENT_MTU;
                    } 

                    // We store the length as octets, also calls the function host to network short to converts the little to big
                    _length = htons((last_fragment_data + sizeof(Header)));
                } 
                else 
                {
                    _length = htons((total_size + sizeof(Header)));
                }
            }

            /// @brief The data stored in this Datagram, it's a virtual pointer to the data after that, so it's required
            /// a lot of caution to handle this pointer in a contiguous way. 
            void * data_address() { return reinterpret_cast<char *>(this) + sizeof(*this);  }

            /// @brief Returns the pointer for the start of a data of a offset Fragment
            void * data_address_with_offset(unsigned int offset) 
            { 
                return reinterpret_cast<char*>(data_address()) + (offset * FRAGMENT_MTU);  
            }

            /// @brief Get the data size of this fragment or datagram
            unsigned int data_size() const
            { 
                if (_flags == FragmentFlags::LAST_FRAGMENT) 
                {
                    return length() - sizeof(Header);
                }

                return FRAGMENT_MTU;
            }

            /// @brief Total length of the original datagram, including Header
            unsigned int total_length() const
            { 
                if (_flags == FragmentFlags::LAST_FRAGMENT) 
                {
                    // Remove the Header of each fragment and use the Header size stored in _length
                    return (_offset * FRAGMENT_MTU) + length();
                }

                // Other fragments already contains the total _length size
                return length();
            }

            /// @brief Quant of fragments required to transfer the datagram
            unsigned int quant_fragments() const
            { 
                if (_flags == FragmentFlags::DATAGRAM) 
                {
                    return 1;
                }

                if (_flags == FragmentFlags::LAST_FRAGMENT) 
                {
                    return _offset + 1;
                }

                return (length() + FRAGMENT_MTU - 1) / FRAGMENT_MTU;
            }

            /// @brief Total size of this object in the memory
            unsigned int object_size() const 
            { 
                if (_flags == FragmentFlags::LAST_FRAGMENT) 
                {
                    return length(); 
                }

                return sizeof(Header) + FRAGMENT_MTU; 
            }

            unsigned short flags() { return _flags; }

            unsigned short id() const { return ntohs(_id); }

            friend Debug & operator<<(Debug & db, const Header & h) {
                db << "{ihl=" << h._ihl
                << ",ver=" << h._version
                << ",tos=" << h._service_type
                << ",len=" << h._length
                << ",lenHeader=" << h.length()
                << ",id="  << h.id()
                << ",flg=" << h._flags
                << ",off=" << h._offset
                << ",ttl=" << h._ttl
                << ",prot=" << h._protocol
                << ",chk=" << h._checksum
                << ",src=" << h._src
                << ",dst=" << h._dst
                << ",data_size=" << h.data_size()
                << ",total_length=" << h.total_length()
                << ",quant_fragments=" << h.quant_fragments()
                << ",object_size=" << h.object_size()
                << "}";

                return db;
            }

        private:
            unsigned short length() const { return ntohs(_length); }

        protected: 
            unsigned char _ihl:4; // IP Header Length
            unsigned char _version:4; // IP Version
            unsigned char _service_type:8; // Type of Service
            unsigned short _length:16;     // Total length of datagram, header + data
            unsigned short _id:16;     // Datagram ID
            unsigned char _flags:3; // Flags (Reserved, DF, MF)
            unsigned short _offset:13;     // Fragment offset
            unsigned char _ttl:4; // Time To Live
            Protocol _protocol;
            unsigned short _checksum:16; // Header checksum (RFC 1071)
            Address _src; // Source IP address
            Address _dst; // Destination IP address
    } __attribute__((packed, may_alias));

    static const unsigned int fragment_mtu() { return Header::FRAGMENT_MTU; }
    
    static const Address broadcast() { return Address::BROADCAST; }
};

class IP : public IPDefinitions
{
private:
    typedef IPDefinitions::MAC_Address MAC_Address;
    typedef IPDefinitions::BufferInfo BufferInfo;
    typedef IPDefinitions::AllocationMap AllocationMap;

public:
    typedef IPDefinitions::Address Address;
    typedef IPDefinitions::Protocol Protocol;
    typedef IPDefinitions::FragmentFlags FragmentFlags;

    static const unsigned int FRAGMENT_MTU = Header::FRAGMENT_MTU;
    
public:
    static IP* init(NIC<Ethernet> * nic);

    /// @brief Send the data directly from the array, it's required to be contiguous
    /// @param dst Destiny Address to send data
    /// @param prot Protocol being used (e.g TCP)
    /// @param data Pointer to the data
    /// @param size Bytes to copy
    void send(const Address & dst, const Protocol & prot, const void * data, unsigned int size);

    /// @brief Sends the data preallocated inside an NonCBuffers accessed by the BuffersHandler class
    /// @param dst Destiny Address to send data
    /// @param prot Protocol being used (e.g TCP)
    /// @param buffer_ptr Pointer returned by the BuffersHandler::alloc_nc() method 
    void send_buffered_data(const Address & dst, const Protocol & prot, void * buffer_ptr);

    SiFiveU_NIC * nic() { return _nic; }

    Address address() { return _address; }

    unsigned int datagrams_received() { return _datagrams_received; }  

protected:
    IP(NIC<Ethernet> * nic);

// Sending Methods
private:
    /// @brief Send the data stored in the AllocationMap with Fragmentation, should not be called if the data can be stored in one Header
    /// @param id Identifier of datagram
    /// @param map AllocationMap returned by the BuffersHandler::alloc_nc() that contains more than one chunk
    void send_buffered_with_fragmentation(const Address & dst, const Protocol & prot, unsigned int id, AllocationMap * map);

    /// @brief Send the contiguous memory to the NIC
    /// @param id Identifier of datagram
    /// @param flags Flags of datagram
    /// @param data Pointer to the start of contiguous data 
    /// @param data_size The data to be stored in the Header (without sizeof(Header))
    void send_data(const Address & dst, const Protocol & prot, unsigned int id, FragmentFlags flags, const void * data, unsigned int data_size); 

    /// @brief Allocate an buffer in the NIC and prepare the Header without copying the data to the Header
    /// @param total_size Total size of the Datagram, including all the fragments (without sizeof(Header))
    /// @param data_size The data to be stored in the Header (without sizeof(Header))
    /// @returns The buffer to be passed to the NIC
    BufferInfo* prepare_header_in_nic_buffer(const Address & dst, const Protocol & prot, unsigned int id, FragmentFlags flags, 
    unsigned int total_size, unsigned int data_size, unsigned int offset);

    /// @brief Get the next Datagram identifier
    static unsigned int get_next_id();

// Receiving Methods
private:
    void handle_fragmentation(Header * fragment);
    void handle_datagram(Header * datagram);

    static void class_nic_callback(BufferInfo * bufferInfo);
    void nic_callback(BufferInfo * bufferInfo);

private:
    MAC_Address convert_ip_to_mac_address(const Address & address);
    Address convert_mac_to_ip_address(const MAC_Address & address);
    Address _address;
    static unsigned int _datagram_count;
    static IP * _ip;
    SiFiveU_NIC * _nic;

    BuffersHandler<char> nw_buffers;

    unsigned int _datagrams_received;

    Queue _queue;
};

__END_SYS

#endif
