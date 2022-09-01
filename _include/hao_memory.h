#ifndef _HAO_MEMORY_H_
#define _HAO_MEMORY_H_

#include <cstddef>

class Memory
{
    public:
        void* AllocMemory(std::size_t size, bool init_zero);
        void FreeMemory(void *ptr);

        static Memory& GetInstance();
        Memory(const Memory&) = delete;
        Memory(Memory&&) = delete;
        Memory& operator=(const Memory&) = delete;
        Memory& operator=(Memory&&) = delete;

    private:
        Memory() = default;
        ~Memory() = default;
};

#endif