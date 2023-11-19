#ifndef __callbacks_wrapper_h
#define __callbacks_wrapper_h

__BEGIN_SYS

template <typename CallbackFunctionType, typename ProtocolType>
class CallbacksWrapper
{
public:
    typedef Simple_List<CallbacksWrapper> List;
    typedef typename List::Element Element;

    CallbacksWrapper(CallbackFunctionType callback, const ProtocolType &prot) : _callback(callback), _link(this),  _protocol(prot) { }

    ProtocolType protocol() const { return _protocol; }
    CallbackFunctionType _callback;

    Element *link() { return &_link; }

protected:
private:
    Element _link;
    ProtocolType _protocol;
};

__END_SYS

#endif