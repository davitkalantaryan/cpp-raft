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
	error=-1,
	ok=0,
	last
};
	
namespace errorType{ enum Type {
	nodeExist = -1
};}

}

namespace connect{
namespace toAnyNode2 {enum Type { 
	newNode = (int)response::last,
	permanentBridge,
	otherLeaderFound,
	last
};}

namespace toLeader2 {enum Type { 
	init = (int)toAnyNode2::last,
	last
};}

namespace toFollower2 {enum Type { 
	init = (int)toLeader2::last,  // this option most probably will not be necessary (renamed to newNode)
	last
};}

namespace fromClient2 {enum Type { 
	allNodesInfo = (int)toFollower2::last, 
	last
};}

} // namespace connect{


namespace receive{

namespace fromAnyNode2 {enum Type { 
	clbkCmd = (int)connect::fromClient2::last,
	last
};}


namespace fromFollower {enum Type { 
	resetPing = (int)fromAnyNode2::last,
	last
};}


namespace fromLeader2 {enum Type { 
	removeNode = (int)fromFollower::last,
	last
};}


namespace fromAdder {enum Type { 
	newNode = (int)fromLeader2::last,
	toNewNodeAddPartitions,
	nodeWithKeyAlreadyExist,
	last
};}


namespace fromNewLeader2 {enum Type { 
	oldLeaderDied = (int)fromAdder::last,
	last
};}


}  // namespace receive{

namespace internal2{

namespace leader {enum Type { 
	removeNode = (int)receive::fromNewLeader2::last,
	last
};}

namespace newLeader {enum Type { 
	becomeLeader = (int)leader::last,
	last
};}

namespace follower {enum Type { 
	newNodeFromLeader = (int)newLeader::last,
	removeNodeRequestFromLeader,
	oldLeaderDied,
	last
};}

}  // namespace internal2{

} // namespace raft{ 

#ifdef _WIN32
#define FILE_DELIMER	'\\'
#else
#define FILE_DELIMER	'/'
#endif

#define FILE_FROM_PATH(_path)	( strrchr((_path),FILE_DELIMER) ? (strrchr((_path),FILE_DELIMER)+1) : (_path)  )

void lock_fprintfLocked(void);
void unlock_fprintfLocked(void);

#ifdef printfWithTime_defined
int fprintfWithTime(FILE* fpFile, const char* a_cpcFormat, ...);
#define printfWithTime(_format,...)	fprintfWithTime(stdout,(_format),__VA_ARGS__)
#else
#define printfWithTime  printf
#define fprintfWithTime  fprintf
#endif

#define DEBUG_APPLICATION_NO_NEW_LINE(_logLevel,...)	\
	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){ \
			lock_fprintfLocked();\
			printfWithTime("fl:%s;ln:%d   ",FILE_FROM_PATH(__FILE__),__LINE__);printf(__VA_ARGS__); \
			unlock_fprintfLocked(); \
		} \
	}while(0)

#define DEBUG_APPLICATION_NO_ADD_INFO(_logLevel,...)	\
	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){lock_fprintfLocked();printf(__VA_ARGS__);unlock_fprintfLocked();}}while(0)

#define DEBUG_APPLICATION_NEW_LINE(_logLevel)	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){printf("\n");}}while(0)

#define DEBUG_APPLICATION(_logLevel,...)	\
	do{ if((_logLevel)<=raft::tcp::g_nLogLevel){ \
			lock_fprintfLocked();\
			printfWithTime("fl:%s;ln:%d   ",FILE_FROM_PATH(__FILE__),__LINE__);printf(__VA_ARGS__);printf("\n"); \
			unlock_fprintfLocked(); \
		} \
	}while(0)

#define ERROR_LOGGING2(...)	\
	do{ \
		lock_fprintfLocked();\
		fprintfWithTime(stderr,"ERROR: fl:%s;ln:%d   ",FILE_FROM_PATH(__FILE__),__LINE__);fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n"); \
		unlock_fprintfLocked(); \
	}while(0)


#define POSSIBLE_BUG(...)	\
	do{ \
		lock_fprintfLocked();\
		fprintfWithTime(stderr,"ERROR: possible bug fl:%s;ln:%d   ",FILE_FROM_PATH(__FILE__),__LINE__);fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n"); \
		unlock_fprintfLocked();\
	}while(0)

#define HANDLE_MEM_DEF2(_pointer,...)				\
	do{ \
		if(!(_pointer)){ \
			lock_fprintfLocked();\
			fprintf(stderr,"fl:%s,ln:%d-> low memory",FILE_FROM_PATH(__FILE__),__LINE__); \
			fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n"); \
			unlock_fprintfLocked();\
			exit(1); \
		} \
	}while(0)

#define HANDLE_MKDIR_BY_NODES(_result,...)	do{if((_result)){ERROR_LOGGING2(__VA_ARGS__);exit((_result));}}while(0)


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
