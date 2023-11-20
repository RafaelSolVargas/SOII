#ifndef __waiting_h
#define __waiting_h

#include "system/config.h"
#include "utility/hash.h"
#include "utility/handler.h"
#include "synchronizer.h"

__BEGIN_SYS

template<typename TId, typename TData>
class WaitingResolutionItemTemplate 
{
private:
    typedef Functor_Handler<WaitingResolutionItemTemplate> Timeout_Handler;

public:
    WaitingResolutionItemTemplate(const TId & id, unsigned int timeout_seconds) : _link(this)
    {
        _resolved = false;
        
        _destroyed = false;

        _id = id;

        _lock = new (SYSTEM) Semaphore(1);

        _sem = new (SYSTEM) Semaphore(0);

        _func_handler = new (SYSTEM) Timeout_Handler(handle_timeout, this);

        _alarm = new (SYSTEM) Alarm(Microsecond(Second(timeout_seconds)), _func_handler);
    }

    ~WaitingResolutionItemTemplate() 
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

    typedef typename Queue<WaitingResolutionItemTemplate>::Element Element;
    typedef Queue<WaitingResolutionItemTemplate> ResolutionQueue;

    Element * link() { return &_link; }

    /// @brief Semaphore to be waited until the net_address is resolved or the alarm is triggered
    Semaphore * semaphore() { return _sem; }

    /// @brief Boolean to check if the address was found
    bool was_resolved() { return _resolved; }

    /// @brief The Net Address that owns the MAC Address that we are looking 
    const TData & data() const { return _response_data; }
    
    /// @brief MAC Address being searched
    const TId & id() const { return _id; }
    
    /// @brief Response the MAC Address  
    /// @param response 
    void resolve_data(const TData & response)  
    { 
        db<WaitingResolutionItemTemplate>(TRC) << "ARP::WaitingItem(" << this << ")::Resolve::Locking " << endl;

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

        _response_data = response;

        _lock->v();

        // Releases the waiting thread
        _sem->v();
    }

    friend Debug & operator<<(Debug & db, const WaitingResolutionItemTemplate & p) {
        db  << "{net_address=" << p._id;

        if (p._resolved) 
        {
            db << "response=" << p._response_data;
        }
            
        db << ",resolved=" << p._resolved
            << ",destroyed=" << p._destroyed
        << "}";
        
        return db;
    }

private:
    static void handle_timeout(WaitingResolutionItemTemplate * item) 
    {
        db<WaitingResolutionItemTemplate>(TRC) << "ARP::WaitingItem(" << item << ")::Timeout::TimeoutReached" << endl;

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
    
    TId _id;
    TData _response_data;
};


__END_SYS

#endif  