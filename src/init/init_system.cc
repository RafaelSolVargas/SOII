// EPOS System Initializer

#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <utility/random.h>
#include <machine.h>
#include <memory.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

class Init_System
{
private:
    static const unsigned int HEAP_SIZE = Traits<System>::HEAP_SIZE;
    static const unsigned int C_BUFFER_SIZE = Traits<System>::CONTIGUOUS_BUFFER_SIZE;
    static const unsigned int NC_BUFFER_SIZE = Traits<System>::NON_CONTIGUOUS_BUFFER_SIZE;

public:
    Init_System() {
        db<Init>(TRC) << "Init_System()" << endl;

        db<Init>(INF) << "Init:si=" << *System::info() << endl;

        db<Init>(INF) << "Initializing the architecture: " << endl;
        CPU::init();

        db<Init>(INF) << "Initializing system's heap: " << endl;
        if(Traits<System>::multiheap) {
            System::_heap_segment = new (&System::_preheap[0]) Segment(HEAP_SIZE, Segment::Flags::SYSD);
            char * heap;
            if(Memory_Map::SYS_HEAP == Traits<Machine>::NOT_USED)
                heap = Address_Space(MMU::current()).attach(System::_heap_segment);
            else
                heap = Address_Space(MMU::current()).attach(System::_heap_segment, Memory_Map::SYS_HEAP);
            if(!heap)
                db<Init>(ERR) << "Failed to initialize the system's heap!" << endl;
            System::_heap = new (&System::_preheap[sizeof(Segment)]) Heap(heap, System::_heap_segment->size());
        } else {
            System::_heap = new (&System::_preheap[0]) Heap(MMU::alloc(MMU::pages(HEAP_SIZE)), HEAP_SIZE);
        }


        // BUG -> Se fizermos primeiro a alocação do contínuo, aparentemente os sizes irão se sobrepor e o grouped_size do CBuffer
        // vai para um valor super alto, imaginando que tem uma faixa praticamente ilimitada de memória (incluindo o NCBuffer que viria logo depois)
       if(Traits<System>::buffer_enable) {
            db<Init>(INF) << "Initializing system's Non Contiguous Buffer: " << endl;
            System::_NCbuffer_segment = new (&System::_preNCbuffer[0]) Segment(NC_BUFFER_SIZE, Segment::Flags::SYSD);
            
            char * nonCBuffer = Address_Space(MMU::current()).attach(System::_NCbuffer_segment);
            
            if(!nonCBuffer) 
            {
                db<Init>(ERR) << "Failed to initialize the system's  Non Contiguous Buffer!" << endl;
            }

            unsigned long segment_size = System::_NCbuffer_segment->size();
            System::_NCbuffer = new (&System::_preNCbuffer[sizeof(Segment)]) NonCBuffer(nonCBuffer, segment_size);
        
        
            db<Init>(INF) << "Initializing system's Contiguous Buffer: " << endl;
            System::_Cbuffer_segment = new (&System::_preCbuffer[0]) Segment(C_BUFFER_SIZE, Segment::Flags::SYSD);
            
            // Uses the constructor that will get the contiguous address to work with the DMA_Buffer
            System::_Cbuffer = new (&System::_preCbuffer[sizeof(Segment)]) CBuffer(System::_Cbuffer_segment->size());
        }  

        db<Init>(INF) << "Initializing the machine: " << endl;
        Machine::init();

        db<Init>(INF) << "Initializing system abstractions: " << endl;
        System::init();

        db<Init>(INF) << "Initializing SiFiveU NIC: " << endl;
        System::_nic = new (SYSTEM) SiFiveU_NIC();

        // Randomize the Random Numbers Generator's seed
        if(Traits<Random>::enabled) {
            db<Init>(INF) << "Randomizing the Random Numbers Generator's seed." << endl;
            if(Traits<TSC>::enabled)
                Random::seed(TSC::time_stamp());

            if(!Traits<TSC>::enabled)
                db<Init>(WRN) << "Due to lack of entropy, Random is a pseudo random numbers generator!" << endl;
        }

        // Initialization continues at init_end
    }
};

// Global object "init_system" must be constructed first.
Init_System init_system;

__END_SYS
