#ifndef __ip_layer_h
#define __ip_layer_h

#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <buffers_handler.h>
#include <machine/nic.h>
#include <network/ethernet.h>
#include <network/arp.h>
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

    // Priority for sending datagrams, will use the Criterion defined for the Threads
    typedef Traits<Thread>::Criterion Priority;
    enum {
        HIGH    = Priority::HIGH,
        NORMAL  = Priority::NORMAL,
        LOW     = Priority::LOW,
    };


    // IP Datagram Parameters
    struct SendingParameters 
    {
        SendingParameters(const Address & dst, const Protocol & prot, const Priority & p = Priority::NORMAL) : destiny(dst), protocol(prot), priority(p) { }

        friend Debug & operator<<(Debug & db, const SendingParameters & p) {
            db << "{dst=" << p.destiny
               << ",prot=" <<  p.protocol
               << ",priority=" <<  p.priority
               << "}";
            return db;
        }

        Address destiny;
        Protocol protocol;
        Priority priority;
    };

    struct Statistics
    {
        typedef unsigned int Count;

        Statistics(): rx_datagrams(0), tx_datagrams(0), next_id(0) {}

        friend Debug & operator<<(Debug & db, const Statistics & s) {
            db << "{rx_datagrams=" << s.rx_datagrams
               << ",tx_datagrams=" <<  s.tx_datagrams
               << ",next_id=" <<  s.next_id
               << "}";
            return db;
        }

        Count rx_datagrams;
        Count tx_datagrams;
        Count next_id;
    };

public:
    class Header
    {
    public: 
        static const unsigned int SERVICE_TYPE = 0;
        static const unsigned int VERSION = 4;
        static const unsigned int HEADER_LENGTH = 5; // 5 bytes
        static const unsigned int TIME_TO_LIVE = 10;

        static const unsigned int FRAGMENT_MTU = NIC<Ethernet>::MTU - (HEADER_LENGTH * 4);

        Header() { }

        /// @brief Creates an Datagram with only the Header
        /// @param src Source IP Address
        /// @param dst Destination IP Address
        /// @param prot Protocol being used
        /// @param total_size Size to be stored in that datagram
        /// @param id Identifier of datagram
        /// @param flags Flag of the datagram
        /// @param offset If is a fragment, the offset
        Header(const Address & src, const Address & dst, const Protocol & prot, unsigned int total_size, 
        unsigned int id, const FragmentFlags & flags, unsigned short offset) 
        : _ihl(HEADER_LENGTH), _version(VERSION),  _service_type(SERVICE_TYPE), _id(htons(id)), _flags(flags), 
            _offset(htons(offset)), _ttl(TIME_TO_LIVE), _protocol(prot), _checksum(0), _src(src), _dst(dst)
        {
            if (flags == FragmentFlags::LAST_FRAGMENT) 
            {
                unsigned int last_fragment_data = (total_size % FRAGMENT_MTU);
                if (last_fragment_data == 0) 
                {
                    last_fragment_data = FRAGMENT_MTU;
                } 

                // Calls the function host to network short to converts the little to big
                _length = htons((last_fragment_data + sizeof(Header)));
            } 
            else 
            {
                _length = htons((FRAGMENT_MTU + sizeof(Header)));
            }
        }

        Address destiny() { return _dst; }
        Address source() { return _src; }
        Protocol protocol() { return _protocol; }

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
            return length() - sizeof(Header);
        }

        /// @brief Total size of this object in the memory
        unsigned int object_size() const 
        { 
            return length();
        }

        unsigned short flags() const { return _flags; }
        unsigned short id() const { return ntohs(_id); }
        unsigned short offset() const { return ntohs(_offset); }

        friend Debug & operator<<(Debug & db, const Header & h) {
            db << "{ihl=" << h._ihl
            << ",ver=" << h._version
            << ",tos=" << h._service_type
            << ",len=" << h.length()
            << ",id="  << h.id()
            << ",flg=" << h.flags()
            << ",off=" << h.offset()
            << ",ttl=" << h._ttl
            << ",prot=" << h._protocol
            << ",chk=" << h._checksum
            << ",src=" << h._src
            << ",dst=" << h._dst
            << ",data_size=" << h.data_size()
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

protected:
    typedef NonCBuffer::AllocationMap AllocationMap;


    // Objects stored in the SendingQueue, they are ordered by the Priority attribute and the Priority can be defined by the User
    class DatagramBufferedRX 
    {
    public:
        DatagramBufferedRX(const SendingParameters & config, MemAllocationMap * map) 
            : _link(this, config.priority), _configuration(config), _allocation_map(map) { }

        DatagramBufferedRX(const SendingParameters & config, MemAllocationMap * map, const Header & header) 
            : _link(this, config.priority), _configuration(config), _allocation_map(map), _header(header), _has_header(true) { }

        typedef typename Queue<DatagramBufferedRX>::Element Element;
    
        typedef Priority Criterion;

        typedef Ordered_Queue<DatagramBufferedRX, Priority, Scheduler<DatagramBufferedRX>::Element> Queue;
        
        Queue::Element * link() { return &_link; }
        SendingParameters config() const { return _configuration; }
        MemAllocationMap * map() { return _allocation_map; }
        Header header() { return _header; }
        bool has_header() { return _has_header; }

    private:
        Queue::Element _link;
        SendingParameters _configuration;
        MemAllocationMap * _allocation_map;
        Header _header;
        bool _has_header;
    };

    typedef DatagramBufferedRX::Queue Queue;

};

class Router;

class IP : public IPDefinitions
{
private:
    typedef IPDefinitions::MAC_Address MAC_Address;
    typedef IPDefinitions::BufferInfo BufferInfo;
    typedef IPDefinitions::AllocationMap AllocationMap;
    typedef IPDefinitions::Statistics Statistics;

public:
    typedef IPDefinitions::Address Address;
    typedef IPDefinitions::Protocol Protocol;
    typedef IPDefinitions::FragmentFlags FragmentFlags;

    static const unsigned int FRAGMENT_MTU = Header::FRAGMENT_MTU;
    
public:
    static IP* init(NIC<Ethernet> * nic);
    ~IP();

    /// @brief Send the data directly from the array, it's required to be contiguous
    /// @param dst Destiny Address to send data
    /// @param prot Protocol being used (e.g TCP)
    /// @param data Pointer to the data
    /// @param size Bytes to copy
    void send(SendingParameters parameters, const void * data, unsigned int size);

    /// @brief Sends the data preallocated inside an NonCBuffers accessed by the BuffersHandler class
    /// @param dst Destiny Address to send data
    /// @param prot Protocol being used (e.g TCP)
    /// @param buffer_ptr Pointer returned by the BuffersHandler::alloc_nc() method 
    void send_buffered_data(SendingParameters parameters, void * buffer_ptr);

    SiFiveU_NIC * nic() { return _nic; }

    Router * router() { return _router; }

    Address address() { return _address; }

    const Statistics & statistics() { return _stats; }

protected:
    IP(NIC<Ethernet> * nic);

// Sending Methods
private:
    /// @brief Send the data stored in the AllocationMap with Fragmentation, should not be called if the data can be stored in one Header
    /// @param map AllocationMap returned by the BuffersHandler::alloc_nc() that contains more than one chunk
    void send_buffered_with_fragmentation(const Address & dst, const Protocol & prot, AllocationMap * map, bool reuse_header, Header header);

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
    BufferInfo* prepare_header_in_nic_buffer(const Address & src, const Address & dst, const Protocol & prot, unsigned int id, FragmentFlags flags, 
    unsigned int total_size, unsigned int data_size, unsigned int offset);

    /// @brief Get the next Datagram identifier
    static unsigned int get_next_id();

// Receiving Methods
private:
    void handle_fragmentation(Header * fragment);
    void handle_datagram(Header * datagram);

    static void class_nic_callback(BufferInfo * bufferInfo);
    void nic_callback(BufferInfo * bufferInfo);

// Methods for sending queue thread
private:
    int sending_queue_function();
    static int class_sending_queue_function();

    void configure_sending_queue();

private:
    Address _address;

    static IP * _ip;
    SiFiveU_NIC * _nic;
    ARP * _arp;

    Statistics _stats;

    Router * _router;

    BuffersHandler<char> nw_buffers;

    /// @brief Queue of DatagramBufferedRX objects to be sended to NIC
    Queue _queue;

    /// @brief Thread to call all the callbacks with the Datagrams that was buffered
    Thread * _sending_queue_thread;
    /// @brief Semaphore to block the execution of the Thread
    Semaphore * _semaphore;
    /// @brief Boolean to determine when the Thread should stop
    bool _deleted;
};

/// @brief An interface with the Internet, Gateways generally contains multiples interfaces, PCs may contains bluethooth, wi-fi and ethernet interfaces
class RouterInterface 
{
public:
    typedef IP::Address Address;
    typedef Simple_List<RouterInterface> InterfacesList;
    typedef InterfacesList::Element Element;

    RouterInterface(ARP * arp, const Address & interface_address, const Address & subnet_mask, const Address & class_mask) 
        : _arp(arp), _interface_address(interface_address), _subnet_mask(subnet_mask), _class_mask(class_mask), _link(this) { }

    Element * link() { return &_link; }

    Address interface_address() { return _interface_address; }
    Address subnet_mask() { return _subnet_mask; }
    Address class_mask() { return _class_mask; }
    ARP * arp() { return _arp; }

    friend Debug & operator<<(Debug & db, const RouterInterface & h) {
        db << "{address=" << h._interface_address
        << ",subnet_mask=" << h._subnet_mask
        << ",class_mask=" << h._class_mask
        << "}";

        return db;
    }

private:
    ARP * _arp;
    Address _interface_address;
    Address _subnet_mask;
    Address _class_mask;

    Element _link;
};

class Routing 
{
public:
    typedef IP::Address Address;
    typedef Simple_List<Routing> RoutingList;
    typedef RoutingList::Element Element;

    Routing(const Address & destiny, const Address & mask, const Address & gateway) : 
        _destiny(destiny), _mask(mask), _gateway(gateway), _link(this) { }

    Element * link() { return &_link; }

    Address destiny() { return _destiny; }
    Address gateway() { return _gateway; }
    Address mask() { return _mask; }

    friend Debug & operator<<(Debug & db, const Routing & h) {
        db << "{destiny=" << h._destiny
        << ",gateway=" << h._gateway
        << ",mask=" << h._mask
        << "}";

        return db;
    }

private:
    Address _destiny;
    Address _mask;
    Address _gateway;

    Element _link;
};

class Router 
{
private:
    typedef Ethernet::Address MAC_Address;
    typedef IP::Address Address;
    typedef RouterInterface::InterfacesList InterfacesList;
    typedef Routing::RoutingList RoutingList;

public:

    Router(ARP * arp, const MAC_Address & mac_addr) : _arp(arp) 
    {
        populate_paths_table(mac_addr);
    }

    MAC_Address route(const Address & dst_addr);

    bool is_gateway() { return _is_gateway; }

    static MAC_Address convert_ip_to_mac_address(const Address & address);
    
    static Address convert_mac_to_ip_address(const MAC_Address & address);

    ~Router();

private:
    Routing * get_routing_of_address(const Address & dst_addr);

    RouterInterface* get_interface_to_same_subnet(const Address & dst_addr);
    
    void populate_paths_table(const MAC_Address & mac_addr);

    InterfacesList _interfaces;
    RoutingList _routings;
    ARP * _arp;
    bool _is_gateway;
};

__END_SYS

#endif
