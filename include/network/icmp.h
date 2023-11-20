#ifndef __icmp_h
#define __icmp_h

#include "network/ip.h"
#include "network/ethernet.h"
#include <network/ip_network.h>
#include <utility/waiting_item.h>

__BEGIN_SYS

class ICMP
{
    friend class IP;
    friend class IPEventsHandler;

protected:
    typedef WaitingResolutionItemTemplate<unsigned int, unsigned int> WaitingResolutionItem;
    typedef WaitingResolutionItem::ResolutionQueue WaitingResolutionQueue;
    static const unsigned int PING_TIMEOUT = 10;

public:
    typedef IP::AllocationMap AllocationMap;
    typedef IP::Header IPHeader;
    typedef IP::Address Address;

    // ICMP Packet Types
    typedef unsigned char Type;
    enum 
    {
        ECHO_REPLY = 0,
        UNREACHABLE = 3,
        ECHO = 8,
        TIME_EXCEEDED = 11
    };

    // ICMP Packet Codes
    typedef unsigned char Code;
    enum {
        DEFAULT = 0,
        TIMEOUT = 1
    };

    // 3 + 1 => Host Unreachable
    // 11 + 1 => Reassembling Timeout

    class Header
    {
    public:
        Header() { }
        Header(const Type & type, const Code & code, unsigned short id, unsigned short seq): 
            _type(type), _code(code), _checksum(0), _id(htons(id)), _sequence(htons(seq)) { }

        Type & type() { return _type; }
        Code & code() { return _code; }
        unsigned short checksum() { return _checksum; }

        unsigned short id() const { return htons(_id); }
        void id(unsigned short id) { _id = htons(id); }

        unsigned short sequence() const { return htons(_sequence); }
        void sequence(unsigned short seq) { _sequence = htons(seq); }

        friend Debug & operator<<(Debug & db, const Header & h) {
            db << "{type=" << h._type
            << ",code=" << h._code
            << ",checksum=" << h._checksum
            << ",id=" << h.id()
            << ",seq="  << h.sequence()
            << "}";

            return db;
        }

    protected:
        Type _type;
        Code _code;
        unsigned short _checksum;
        unsigned short _id;
        unsigned short _sequence;
    } __attribute__((packed));

    // Packages ICMP has the original IPHeader and the first 64 bit of his data
    static const unsigned int PKG_DATA_SIZE = sizeof(IPHeader) + 64; 

    typedef unsigned char Data[PKG_DATA_SIZE];

    class Packet: public Header
    {
    public:
        Packet(const Type & type, const Code & code, unsigned short id, unsigned short seq): Header(type, code, id, seq) { }

        void sum() { _checksum = 0; _checksum = htons(IP::generic_checksum(reinterpret_cast<unsigned char *>(this), sizeof(Packet))); }
        bool check() { return (IP::generic_checksum(reinterpret_cast<unsigned char *>(this), sizeof(Packet)) != 0xffff); }

    private:
        Data _data;
    } __attribute__((packed));

protected:
    ICMP(IP * ip);
    static void class_datagram_callback(IPHeader * header, AllocationMap * map);
    void datagram_callback(IPHeader * header, AllocationMap * map);

    void answer_ping(IPHeader * ipHeader, Header * header);

    void process_event(IPEventsHandler::IPEvent event, IPHeader * ipHeader);

public:
    ~ICMP() { }

    void ping(Address dst_address);

private:
    static ICMP * _instance;
    IP * _ip;

    WaitingResolutionQueue _waiting_reply_queue;
};

__END_SYS

#endif