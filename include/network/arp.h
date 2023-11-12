#ifndef __arp_h
#define __arp_h

#include "machine/riscv/sifive_u/sifiveu_nic.h"
#include "ethernet.h"
#include "utility/hash.h"
#include "utility/handler.h"
#include "time.h"
#include "synchronizer.h"

__BEGIN_SYS

class ARPDefinitions 
{
protected:
    typedef SiFiveU_NIC::Address MAC_Address;
    typedef NIC_Common::Address<4> Net_Address;

    static const unsigned int Net_Protocol = Ethernet::PROTO_IP;
    static const unsigned int NET_ENTRIES = 10;

    // ARP Protocol for NIC
    static const unsigned int PROTOCOL = Ethernet::PROTO_ARP;

    static const unsigned int ARP_TIMEOUT = 15;
    static const unsigned int ARP_TRIES = 4;

    class WaitingResolutionItem 
    {
    private:
        typedef Functor_Handler<WaitingResolutionItem> Timeout_Handler;

    public:
        WaitingResolutionItem(const Net_Address & net_address) : _link(this)
        {
            _resolved = false;
            
            _destroyed = false;

            _searched_address = net_address;

            _lock = new (SYSTEM) Semaphore(1);

            _sem = new (SYSTEM) Semaphore(0);

            _func_handler = new (SYSTEM) Timeout_Handler(handle_timeout, this);

            _alarm = new (SYSTEM) Alarm(Microsecond(Second(ARP_TIMEOUT)), _func_handler);
        }

        ~WaitingResolutionItem() 
        {
            delete _lock;
            
            delete _sem;

            delete _func_handler;

            // If resolved then the resolve had already deleted the alarm
            if (!_resolved) 
            {
                delete _alarm;
            }
        }

        typedef typename Queue<WaitingResolutionItem>::Element Element;
        typedef Queue<WaitingResolutionItem> ResolutionQueue;

        Element * link() { return &_link; }
    
        /// @brief Semaphore to be waited until the net_address is resolved or the alarm is triggered
        Semaphore * semaphore() { return _sem; }

        /// @brief Boolean to check if the address was found
        bool was_resolved() { return _resolved; }

        /// @brief The Net Address that owns the MAC Address that we are looking 
        const Net_Address & searched_address() const { return _searched_address; }
        
        /// @brief MAC Address being searched
        const MAC_Address & response_address() const { return _response_address; }
        
        /// @brief Response the MAC Address  
        /// @param response 
        void resolve_address(const MAC_Address & response)  
        { 
            db<WaitingResolutionItem>(TRC) << "ARP::WaitingItem(" << this << ")::Resolve::Locking " << endl;

            _lock->p();
            // In this case the alarm was already triggered
            if (_destroyed == true) 
            {
                _lock->v();

                return;
            }

            // Delete the alarm to stop it from trigger
            delete _alarm;

            _resolved = true;
    
            _response_address = response;

            _lock->v();

            // Releases the waiting thread
            _sem->v();
        }

        friend Debug & operator<<(Debug & db, const WaitingResolutionItem & p) {
            db  << "{net_address=" << p._searched_address;

            if (p._resolved) 
            {
                db << "response=" << p._response_address;
            }
                
            db << ",resolved=" << p._resolved
                << ",destroyed=" << p._destroyed
            << "}";
            
            return db;
        }

    private:
        static void handle_timeout(WaitingResolutionItem * item) 
        {
            db<WaitingResolutionItem>(TRC) << "ARP::WaitingItem(" << item << ")::Timeout::TimeoutReached" << endl;

            
            item->_lock->p();


            // If the response arrive and accessed the Mutex before the TimeoutHandler
            if (item->_resolved == true) 
            {
                item->_lock->v();

                return;
            }

            item->_destroyed = true;

            item->_lock->v();

            item->_sem->v();
        }

        Element _link;

        Semaphore * _sem;
        Timeout_Handler * _func_handler;
        Alarm * _alarm;
        Semaphore * _lock;

        bool _resolved;
        bool _destroyed;
        
        Net_Address _searched_address;
        MAC_Address _response_address;
    };

protected:
    typedef WaitingResolutionItem::ResolutionQueue WaitingResolutionQueue;

public:

    typedef unsigned short Opcode;
    enum 
    {
        REQUEST = 1, // ARP Request
        REPLY = 2 // ARP reply
    };

    typedef unsigned short HType;
    enum 
    {
        ETHERNET = 1
    };

    class Message
    {
    public:
        Message(Opcode op, const MAC_Address & src_mac_addr, const Net_Address & src_net_addr, const MAC_Address & dst_mac_addr, const Net_Address & dst_net_addr)
        : _mac_type(htons(ETHERNET)), _prot(htons(Net_Protocol)), _mac_length(sizeof(MAC_Address)), _net_length(sizeof(Net_Address)), 
          _opcode(htons(op)), _src_mac_addr(src_mac_addr), _src_net_addr(src_net_addr), _dst_mac_addr(dst_mac_addr), _dst_net_addr(dst_net_addr) 
        { }

        Opcode opcode() const { return ntohs(_opcode); }
        const MAC_Address & src_mac_addr() const { return _src_mac_addr; }
        const Net_Address & src_net_addr() const { return _src_net_addr; }
        const MAC_Address & dst_mac_addr() const { return _dst_mac_addr; }
        const Net_Address & dst_net_addr() const { return _dst_net_addr; }

        friend OStream & operator<<(OStream & os, const Message & p) {
            os  << "{mac_type=" << ntohs(p._mac_type)
                << ",ptype=" << hex << ntohs(p._prot) << dec
                << ",mac_length=" << p._mac_length
                << ",net_length=" << p._net_length
                << ",opcode=" << ntohs(p._opcode)
                << ",src_mac_addr=" << p._src_mac_addr
                << ",src_net_addr=" << p._src_net_addr
                << ",dst_mac_addr=" << p._dst_mac_addr
                << ",dst_net_addr=" << p._dst_net_addr 
            << "}";
            
            return os;
        }

    private:
        unsigned short _mac_type:16; // Hardware Type
        unsigned short _prot:16; // Net Protocol Type
        unsigned char  _mac_length:8;  // Hardware Address Length
        unsigned char  _net_length:8;  // Protocol Address Length
        unsigned short _opcode:16;  // Opcode (1 -> Request | 2 -> Response)
        MAC_Address _src_mac_addr;   // Source MAC Address
        Net_Address _src_net_addr;   // Source Internet Address
        MAC_Address _dst_mac_addr;   // Destiny MAC Address
        Net_Address _dst_net_addr;   // Destiny Internet Address
    } __attribute__((packed));

protected:
    class TableEntry 
    {
    public:
        TableEntry(const Net_Address & net_address, const MAC_Address & mac_address) 
            : _link(this, net_address), _net_address(net_address), _mac_address(mac_address) 
        { }

        typedef Simple_Hash<TableEntry, NET_ENTRIES, Net_Address> ResolutionTable;

        typedef ResolutionTable::Element Element;
        
        Element * link() { return &_link; }
        const Net_Address & net_address() { return _net_address; }
        const MAC_Address & mac_address() const { return _mac_address; }
 
    private:
        Element _link;
        Net_Address _net_address;
        MAC_Address _mac_address;
    };

    typedef TableEntry::ResolutionTable ResolutionTable;
};


class ARP : public ARPDefinitions
{
private:
    typedef ARPDefinitions::ResolutionTable ResolutionTable;
    typedef ARPDefinitions::TableEntry TableEntry;

    typedef Ethernet::BufferInfo BufferInfo;

public:
    typedef ARPDefinitions::MAC_Address MAC_Address;
    typedef ARPDefinitions::Net_Address Net_Address;

    static ARP* init(SiFiveU_NIC * nic, const Net_Address & address);

    /// @brief Returns the MAC_Address of a net_Address, may use the value from a ResolutionTable
    /// or send a ARP message by the NIC to restore it
    /// @param net_address 
    /// @return The MAC_Address of the destination net_address
    MAC_Address resolve(const Net_Address & searched_address);

    Net_Address net_address() { return _net_address; }
    MAC_Address mac_address() { return _mac_address; }

protected:
    ARP(SiFiveU_NIC * nic, const Net_Address & address);

// Receiving Methods
private:
    // Handles the incoming of ARP messages
    static void class_nic_callback(BufferInfo * bufferInfo);
    void nic_callback(BufferInfo * bufferInfo);

    void send_request(Net_Address searched_address);
    void respond_request(Message * request);
    void handle_reply(Message * request);
    
private:
    static ARP * _arp;
    
    SiFiveU_NIC * _nic;

    MAC_Address _mac_address;
    Net_Address _net_address;

    ResolutionTable _resolutionTable;
    WaitingResolutionQueue _resolutionQueue;
};

__END_SYS

#endif
