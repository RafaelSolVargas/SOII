// EPOS Application Initializer

#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <machine/riscv/sifive_u/sifiveu_observer.h>
#include <architecture.h>
#include <utility/heap.h>
#include <machine.h>
#include <system.h>

extern "C" char _end; // defined by GCC

__BEGIN_SYS

class Init_Application
{
private:
    static const unsigned int NIC_ENABLED = Traits<SiFiveU_NIC>::enabled;
    static const unsigned int C_BUFFER_SIZE = Traits<System>::CONTIGUOUS_BUFFER_SIZE;
    static const unsigned int NC_BUFFER_SIZE = Traits<System>::NON_CONTIGUOUS_BUFFER_SIZE;
    static const unsigned int HEAP_SIZE = Traits<Application>::HEAP_SIZE;
    static const unsigned int STACK_SIZE = Traits<Application>::STACK_SIZE;

public:
    Init_Application() {
        db<Init>(TRC) << "Init_Application()" << endl;

        // Initialize Application's heap
        db<Init>(INF) << "Initializing application's heap: ";
        if(Traits<System>::multiheap) { // heap in data segment arranged by SETUP
            db<Init>(INF) << endl;
            char * heap = (MMU::align_page(&_end) >= CPU::Log_Addr(Memory_Map::APP_DATA)) ? MMU::align_page(&_end) : CPU::Log_Addr(Memory_Map::APP_DATA); // ld is eliminating the data segment in some compilations, particularly for RISC-V, and placing _end in the code segment
            Application::_heap = new (&Application::_preheap[0]) Heap(heap, HEAP_SIZE);
        } else {
            db<Init>(INF) << "adding all free memory to the unified system's heap!" << endl;
            for(unsigned int frames = MMU::allocable(); frames; frames = MMU::allocable())
                System::_heap->free(MMU::alloc(frames), frames * sizeof(MMU::Page));
        }

       if(Traits<System>::buffer_enable) {
            db<Init>(INF) << "Initializing system's Contiguous Buffer: " << endl;
            // Uses the constructor that will get the contiguous address to work with the DMA_Buffer
            System::_Cbuffer = new (SYSTEM) CBuffer(C_BUFFER_SIZE);


            db<Init>(INF) << "Initializing system's Non Contiguous Buffer: " << endl;
            Segment * nc_buffer_segment = new (SYSTEM) Segment(NC_BUFFER_SIZE, Segment::Flags::SYSD);
            
            char * address = Address_Space(MMU::current()).attach(nc_buffer_segment);
            if(!address) 
            {
                db<Init>(ERR) << "Failed to initialize the system's  Non Contiguous Buffer!" << endl;
            }

            System::_NCbuffer = new (SYSTEM) NonCBuffer(address, nc_buffer_segment->size());
        }  

        db<Init>(INF) << "Initializing SiFiveU NIC: " << endl;
        System::_nic = new (SYSTEM) SiFiveU_NIC();

        // Cria uma instância de observador para a NIC
        NIC_Observer * observer = new (SYSTEM) NIC_Observer();

        // Avisa para a NIC que temos uma classe observando ela pelo protocolo IP
        System::_nic->attach(observer, Ethernet::PROTO_IP);
        }
};

// Global object "init_application"  must be linked to the application (not to the system) and there constructed at first.
Init_Application init_application;

__END_SYS
