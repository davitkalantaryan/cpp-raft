//
// File       -> raft_macroses_and_functions.h
// Creaed on  -> 2018 May 05
// to include -> #include "raft_macroses_and_functions.h"
//

#ifndef __raft_macroses_and_functions_h__
#define __raft_macroses_and_functions_h__

#include <stdlib.h>
#include <stdio.h>

#if defined(_MSC_VER)
#define SWAP4BYTES(_x)	(_x)=_byteswap_ulong ((_x))
#elif defined(__GNUC__)
#define SWAP4BYTES		__builtin_bswap32
#else
#endif

#ifndef HANDLE_MEM_DEF
#define HANDLE_MEM_DEF(...)				\
	do{ \
		fprintf(stderr,"fl:%s,ln:%d-> low memory",__FILE__,__LINE__);fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");exit(1); \
	}while(0)
#endif

#define ERR_REP(...)					fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n")
#define LOG_REP(...)					fprintf(stdout,__VA_ARGS__);fprintf(stdout,"\n")


#ifdef __cplusplus

#define OVERRIDE	override

namespace dynd{
	namespace Request{enum Type{otherServer,clientAdd,clientQuery,clientDelete};}
}

namespace raft{ 

namespace connect{
namespace anyNode {enum Type { 
	newNode = 0 ,
	raftBridge,
	dataBridge,
	last
};}

namespace leader {enum Type { 
	newNode = (int)anyNode::last,
	last
};}

namespace follower {enum Type { 
	newNode2 = (int)leader::last,  // this option most probably will not be necessary (renamed to newNode2)
	last
};}

} // namespace connect{


namespace receive{

namespace anyNode {enum Type { 
	clbkCmd = 0 ,
	last
};}

namespace follower {enum Type { 
	newNode = (int)anyNode::last ,
	last
};}

namespace leader {enum Type { 
	newNode = (int)follower::last,
	last
};}


}  // namespace receive{

} // namespace raft{ 

#endif  // #ifdef __cplusplus


#endif  // #ifndef __raft_macroses_and_functions_h__
