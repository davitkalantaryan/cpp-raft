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
#include <signal.h>
#include <cpp11+/shared_mutex_cpp14.hpp>


#if defined(_WIN32) && !defined(_WLAC_USED)
struct sigaction { void(*sa_handler)(int); };
#define sigaction(_sigNum,_newAction,_oldAction)	\
	do{ \
		if((_oldAction)){(_oldAction)->sa_handler=signal((_sigNum),(_newAction)->sa_handler);} \
		else{signal((_sigNum),(_newAction)->sa_handler);} \
	}while(0)
#endif

#define NULL_ACTION	((struct sigaction*)0)

#ifdef _WIN32
#else
#include <sys/types.h>
#include <sys/syscall.h>
#define GetCurrentThreadId()    syscall(__NR_gettid)
#endif

#ifdef _WIN32
#else
#ifndef HANDLE_SIG_ACTIONS
#define HANDLE_SIG_ACTIONS
#endif
#endif

#if defined(_MSC_VER)
#define SWAP4BYTES(_x)	(_x)=_byteswap_ulong ((_x))
#elif defined(__GNUC__)
//#define SWAP4BYTES		__builtin_bswap32
void raft_swap_4_bytes(int32_t& a_unValue);
#define SWAP4BYTES  raft_swap_4_bytes
#else
#endif


#ifdef __cplusplus

#ifndef OVERRIDE
#define OVERRIDE	override
#endif

namespace raft{ 

namespace tcp{
extern int g_nLogLevel;
}

namespace response {

enum Type{
	ok,
	last
};
	
namespace error{ enum Type {
	nodeExist = -1
};}

}

namespace connect{
namespace toAnyNode {enum Type { 
	newNode = (int)response::last,
	raftBridge,
	dataBridge,
	otherLeaderFound,
	last
};}

namespace toLeader {enum Type { 
	newNode = (int)toAnyNode::last,
	last
};}

namespace toFollower {enum Type { 
	newNode = (int)toLeader::last,  // this option most probably will not be necessary (renamed to newNode)
	last
};}

namespace fromClient {enum Type { 
	allNodesInfo = (int)toFollower::last,
	last
};}

} // namespace connect{


namespace receive{

namespace anyNode {enum Type { 
	clbkCmd = (int)connect::fromClient::last,
	last
};}

namespace follower {enum Type { 
	newNode = (int)anyNode::last ,
	last
};}


namespace fromLeader {enum Type { 
	newNode = (int)follower::last,
	removeNode,
	oldLeaderDied,
	last
};}


namespace fromNewLeader {enum Type { 
	oldLeaderDied = (int)fromLeader::last,
	last
};}


}  // namespace receive{


namespace leaderInternal {enum Type { 
	newNode = (int)receive::fromNewLeader::last,
	removeNode,
	last
};}

namespace newLeaderInternal {enum Type { 
	becomeLeader = (int)leaderInternal::last,
	last
};}

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

#define ERROR_LOGGING2(...)	\
	do{ fprintf(stderr,"ERROR: fl:%s;ln:%d   ",FILE_FROM_PATH(__FILE__),__LINE__);fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");}while(0)

#define HANDLE_MEM_DEF2(_pointer,...)				\
	do{ \
		if(!(_pointer)){ \
			fprintf(stderr,"fl:%s,ln:%d-> low memory",FILE_FROM_PATH(__FILE__),__LINE__); \
			fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");exit(1); \
		} \
	}while(0)


#if 0
#define DEBUG_HANGING(...)  \
    do{ \
        std::string aStr="testString"; \
        printf("!!!!!!!!!!!!!! fl:%s;ln:%d (%s)\n",FILE_FROM_PATH(__FILE__),__LINE__,aStr.c_str()); \
    }while(0)
#endif

#define DEBUG_HANGING(...)

#endif  // #ifdef __cplusplus


#endif  // #ifndef __raft_macroses_and_functions_h__
