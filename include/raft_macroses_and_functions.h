//
// File       -> raft_macroses_and_functions.h
// Creaed on  -> 2018 May 05
// to include -> #include "raft_macroses_and_functions.h"
//

#ifndef __raft_macroses_and_functions_h__
#define __raft_macroses_and_functions_h__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

namespace tcp{
extern int g_nLogLevel;
}

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
	removeNode,
	oldLeaderDied,
	last
};}


}  // namespace receive{

} // namespace raft{ 

#ifdef _WIN32
#define FILE_DELIMER	'\\'
#else
#define FILE_DELIMER	'/'
#endif

#define FILE_FROM_PATH(_path)	( strrchr((_path),FILE_DELIMER) ? (strrchr((_path),FILE_DELIMER)+1) : (_path)  )

#define DEBUG_APPLICATION_NO_NEW_LINE(_logLevel,...)	\
	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){printf("fl:%s;ln:%d   ",FILE_FROM_PATH(__FILE__),__LINE__);printf(__VA_ARGS__);}}while(0)

#define DEBUG_APPLICATION_NO_ADD_INFO(_logLevel,...)	\
	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){printf(__VA_ARGS__);}}while(0)

#define DEBUG_APPLICATION_NEW_LINE(_logLevel)	\
	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){printf("\n");}}while(0)

#define DEBUG_APPLICATION(_logLevel,...) do{DEBUG_APPLICATION_NO_NEW_LINE(_logLevel,__VA_ARGS__);DEBUG_APPLICATION_NEW_LINE(_logLevel);}while(0)

#endif  // #ifdef __cplusplus


#endif  // #ifndef __raft_macroses_and_functions_h__
