//
// raft_common_functions.cpp
//

#include <stdint.h>

#ifndef _MSC_VER

void raft_swap_4_bytes(int32_t& a_unValue)
{
    char* pcVector = (char*)&a_unValue;
    char cTemp = pcVector[0];
    pcVector[0]=pcVector[3];
    pcVector[0]=cTemp;

    cTemp = pcVector[1];
    pcVector[1]=pcVector[2];
    pcVector[2]=cTemp;
}

void raft_swap_4_bytes(uint32_t& a_unValue)
{
    char* pcVector = (char*)&a_unValue;
    char cTemp = pcVector[0];
    pcVector[0]=pcVector[3];
    pcVector[0]=cTemp;

    cTemp = pcVector[1];
    pcVector[1]=pcVector[2];
    pcVector[2]=cTemp;
}


#endif // #ifndef _MSC_VER
