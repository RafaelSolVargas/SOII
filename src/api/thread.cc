// EPOS Thread Implementation

#include <machine.h>
#include <system.h>
#include <process.h>

// This_Thread class attributes
__BEGIN_UTIL
bool This_Thread::_not_booting;
__END_UTIL

__BEGIN_SYS

Scheduler_Timer *Thread::_timer;

Thread *volatile Thread::_running;
Thread::Queue Thread::_ready;
Thread::Queue Thread::_suspended;
Thread *Thread::_idleThread;

void Thread::constructor_prologue(unsigned int stack_size)
{
    lock();

    _stack = reinterpret_cast<char *>(kmalloc(stack_size));
}

void Thread::constructor_epilogue(Log_Addr entry, unsigned int stack_size)
{
    db<Thread>(TRC) << "Thread(entry=" << entry
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",s=" << stack_size
                    << "},context={b=" << _context
                    << "," << *_context << "}) => " << this << endl;

    switch (_state)
    {
    case RUNNING:
        break;
    case SUSPENDED:
        _suspended.insert(&_link);
        break;
    default:
        _ready.insert(&_link);
    }

    unlock();
}

Thread::~Thread()
{
    lock();

    db<Thread>(TRC) << "~Thread(this=" << this
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",context={b=" << _context
                    << "," << *_context << "})" << endl;

    _ready.remove(this);
    _suspended.remove(this);

    if (_waiting)
        _waiting->remove(this);

    if (_joining)
        _joining->resume();

    if (_idleThread)
        delete _idleThread;

    unlock();

    kfree(_stack);
}

int Thread::join()
{
    lock();

    db<Thread>(TRC) << "Thread::join(this=" << this << ",state=" << _state << ")" << endl;

    // Precondition: no Thread::self()->join()
    assert(running() != this);

    // Precondition: a single joiner
    assert(!_joining);

    if (_state != FINISHING)
    {
        _joining = running();
        _joining->suspend();
    }
    else
        unlock();

    return *reinterpret_cast<int *>(_stack);
}

void Thread::pass()
{
    lock();

    db<Thread>(TRC) << "Thread::pass(this=" << this << ")" << endl;

    Thread *prev = _running;
    prev->_state = READY;
    _ready.insert(&prev->_link);

    _ready.remove(this);
    _state = RUNNING;
    _running = this;

    dispatch(prev, this);

    unlock();
}

void Thread::suspend()
{
    lock();

    db<Thread>(TRC) << "Thread::suspend(this=" << this << ")" << endl;

    if (_running != this)
        _ready.remove(this);

    _state = SUSPENDED;
    _suspended.insert(&_link);

    if (_running == this)
    {
        _running = get_next_running_thread();

        dispatch(this, _running);
    }

    unlock();
}

void Thread::resume()
{
    lock();

    db<Thread>(TRC) << "Thread::resume(this=" << this << ")" << endl;

    _suspended.remove(this);
    _state = READY;
    _ready.insert(&_link);

    unlock();
}

// Class methods
void Thread::yield()
{
    lock();

    db<Thread>(TRC) << "Thread::yield(running=" << _running << ")" << endl;

    Thread *prev = _running;
    prev->_state = READY;

    // Don't insert idleThread in queue
    if (!prev->is_idle_thread)
    {
        _ready.insert(&prev->_link);
    }

    _running = get_next_running_thread();

    dispatch(prev, _running);

    unlock();
}

void Thread::exit(int status)
{
    lock();

    db<Thread>(TRC) << "Thread::exit(status=" << status << ") [running=" << running() << "]" << endl;

    Thread *prev = _running;
    prev->_state = FINISHING;
    *reinterpret_cast<int *>(prev->_stack) = status;

    if (prev->_joining)
    {
        Thread *joining = prev->_joining;
        prev->_joining = 0;
        joining->resume(); // implicit unlock()
        lock();
    }

    // If both queue empty then we finish the execution
    if (_ready.empty() && _suspended.empty())
    {
        db<Thread>(WRN) << "The last thread has exited!" << endl;
        if (reboot)
        {
            db<Thread>(WRN) << "Rebooting the machine ..." << endl;
            Machine::reboot();
        }
        else
        {
            db<Thread>(WRN) << "Halting the CPU ..." << endl;
            CPU::halt();
        }
    }
    else
    {
        _running = get_next_running_thread();

        dispatch(prev, _running);
    }

    unlock();
}

// Always return a Thread to be scheduled, could be a Thread from the _ready queue or the idleThread
// Already sets the thread state to RUNNING
Thread *Thread::get_next_running_thread()
{
    assert(locked());

    // If someone's ready, returns it
    if (!_ready.empty())
    {
        Thread *next = _ready.remove()->object();
        next->_state = RUNNING;

        return next;
    }
    else
    {
        _idleThread->_state = RUNNING;

        return _idleThread;
    }
}

void Thread::sleep(Queue *q)
{
    db<Thread>(TRC) << "Thread::sleep(running=" << running() << ",q=" << q << ")" << endl;

    assert(locked()); // locking handled by caller

    Thread *prev = running();
    prev->_state = WAITING;
    prev->_waiting = q;
    q->insert(&prev->_link);

    _running = get_next_running_thread();

    dispatch(prev, _running);
}

void Thread::wakeup(Queue *q)
{
    db<Thread>(TRC) << "Thread::wakeup(running=" << running() << ",q=" << q << ")" << endl;

    assert(locked()); // locking handled by caller

    if (!q->empty())
    {
        Thread *t = q->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _ready.insert(&t->_link);
    }
}

void Thread::wakeup_all(Queue *q)
{
    db<Thread>(TRC) << "Thread::wakeup_all(running=" << running() << ",q=" << q << ")" << endl;

    assert(locked()); // locking handled by caller

    while (!q->empty())
    {
        Thread *t = q->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _ready.insert(&t->_link);
    }
}

void Thread::reschedule()
{
    yield();
}

void Thread::time_slicer(IC::Interrupt_Id i)
{
    reschedule();
}

void Thread::dispatch(Thread *prev, Thread *next)
{
    if (prev != next)
    {
        assert(prev->_state != RUNNING);
        assert(next->_state == RUNNING);

        db<Thread>(TRC) << "Thread::dispatch(prev=" << prev << ",next=" << next << ")" << endl;
        db<Thread>(INF) << "prev={" << prev << ",ctx=" << *prev->_context << "}" << endl;
        db<Thread>(INF) << "next={" << next << ",ctx=" << *next->_context << "}" << endl;

        // The non-volatile pointer to volatile pointer to a non-volatile context is correct
        // and necessary because of context switches, but here, we are locked() and
        // passing the volatile to switch_constext forces it to push prev onto the stack,
        // disrupting the context (it doesn't make a difference for Intel, which already saves
        // parameters on the stack anyway).
        CPU::switch_context(const_cast<Context **>(&prev->_context), next->_context);
    }
}

int Thread::idle()
{
    db<Thread>(TRC) << "Thread::idle()" << endl;

    db<Thread>(INF) << "There are no runnable threads at the moment!" << endl;
    db<Thread>(INF) << "Halting the CPU ..." << endl;

    unlock();
    CPU::halt();

    return 0;
}

__END_SYS

// Id forwarder to the spin lock
__BEGIN_UTIL

volatile CPU::Reg This_Thread::id()
{
    return _not_booting ? CPU::Reg(Thread::self()) : CPU::Reg(CPU::id() + 1);
}

__END_UTIL
