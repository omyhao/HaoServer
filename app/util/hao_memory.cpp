#include "hao_memory.h"
#include "hao_log.h"
#include <cstdlib>
using namespace hao_log;

Memory& Memory::GetInstance()
{
    static Memory memory_;
    return memory_;
}

void* Memory::AllocMemory(std::size_t size, bool init_zero)
{
    if(init_zero)
    {
        return std::calloc(size, sizeof(char));
    }
    else
    {
        return std::malloc(size);
    }
}

void Memory::FreeMemory(void *ptr)
{
    
    std::free(ptr);
}
