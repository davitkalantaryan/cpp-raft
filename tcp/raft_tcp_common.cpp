//
// file:		raft_tcp_common.cpp
// created on:	2018 May 11
//

#include "raft_tcp_common.hpp"
#include <memory.h>
#include "raft_macroses_and_functions.h"
#include <cpp11+/mutex_cpp11.hpp>
#include <common/newlockguards.hpp>
#include <string.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif


bool raft::tcp::NodeIdentifierKey::operator==(const NodeIdentifierKey& a_aM)const
{
	return (memcmp(this, &a_aM, sizeof(a_aM)) == 0);
}


bool raft::tcp::NodeIdentifierKey::isSame(const char* a_ip4Address, int32_t a_port)const
{
	if(this->port != a_port){return false;}

	NodeIdentifierKey aSecond;
	aSecond.port = a_port;
	aSecond.set_ip4Address1(a_ip4Address);

	return aSecond == (*this);
}


void raft::tcp::NodeIdentifierKey::generateKey(const char* a_ip4Address, int32_t a_port, std::string* a_pKey)
{
	NodeIdentifierKey nodeKey;
	nodeKey.set_ip4Address1(a_ip4Address);
	nodeKey.port = a_port;
	a_pKey->resize(sizeof(NodeIdentifierKey));
	memcpy(const_cast<char*>(a_pKey->data()),&nodeKey, sizeof(NodeIdentifierKey));
}


void raft::tcp::NodeIdentifierKey::set_ip4Address1(const std::string& a_hostName)
{
	//memset(this->ip4Address, 0, MAX_IP4_LEN);
	//strncpy(this->ip4Address, a_hostName.c_str(), MAX_IP4_LEN);
	const char* ipAddress=common::socketN::GetIp4AddressFromHostName(a_hostName.c_str());
	if(!ipAddress){ipAddress=a_hostName.c_str();}

	memset(this->ip4Address, 0, MAX_IP4_LEN);
	if (strcmp(ipAddress, "127.0.0.1") == 0) {
		common::socketN::GetOwnIp4Address(this->ip4Address, MAX_IP4_LEN);
	}
	else {
		strncpy(this->ip4Address, ipAddress, MAX_IP4_LEN);
	}
}


void raft::tcp::NodeIdentifierKey::set_ip4Address2(const sockaddr_in* a_remoteAddr)
{
	memset(this->ip4Address, 0, MAX_IP4_LEN);
	if (strcmp(common::socketN::GetIPAddress(a_remoteAddr), "127.0.0.1") == 0) {
		common::socketN::GetOwnIp4Address(this->ip4Address, MAX_IP4_LEN);
	}
	else{
		strncpy(this->ip4Address,common::socketN::GetIPAddress(a_remoteAddr), MAX_IP4_LEN);
	}

}


void raft::tcp::NodeIdentifierKey::set_addressAndPort(char* a_addressAndPort, int32_t a_defaultPort)
{
	char* pcPortStart = strchr(a_addressAndPort, ':');
	
	if(pcPortStart){
		*pcPortStart = 0;
		this->port = atoi(pcPortStart + 1);
	}
	else {this->port=a_defaultPort;}
	this->set_ip4Address1(a_addressAndPort);
}


namespace raft{namespace tcp{

const char g_ccResponceOk= response::ok;
const int32_t g_cnResponceOk = response::ok;
int g_nLogLevel = 0;

bool ConnectAndGetEndian(common::SocketTCP* a_pSock, const NodeIdentifierKey& a_nodeInfo,char a_cRequest, uint32_t* a_pIsEndianDiffer, int a_nSockTimeout)
{
	int nSndRcv;
	uint16_t unRemEndian;

	if(a_pSock->connectC(a_nodeInfo.ip4Address, a_nodeInfo.port,500)){
		a_pSock->closeC();
		DEBUG_APP_WITH_NODE(4,&a_nodeInfo,"Unable to connect");
		return false;
	}
	a_pSock->setTimeout(a_nSockTimeout);

	nSndRcv = a_pSock->readC(&unRemEndian, 2);
	if(nSndRcv!=2){
		a_pSock->closeC();
		DEBUG_APP_WITH_NODE(3,&a_nodeInfo, "Unable to get endian. retCode=%d", nSndRcv);
		return false;
	}
	if(unRemEndian==1){*a_pIsEndianDiffer=0;}
	else {*a_pIsEndianDiffer = 1;}

	//cRequest = a_connectionCode;
	nSndRcv = a_pSock->writeC(&a_cRequest,1);
	if (nSndRcv != 1) { 
		a_pSock->closeC(); 
		DEBUG_APP_WITH_NODE(1,&a_nodeInfo, "Unable to send request");
		return false; 
	}

	a_pSock->setTimeout(SOCK_TIMEOUT_MS);
	return true;
}


void SendErrorWithString(::common::SocketTCP& a_clientSock, const char* a_cpcErrorString)
{
	int32_t nResponse = (int32_t)raft::response::error;
	a_clientSock.writeC(&nResponse, 4);
	nResponse = (int32_t)strlen(a_cpcErrorString);
	a_clientSock.writeC(&nResponse, 4);
	a_clientSock.writeC(a_cpcErrorString, nResponse);
}


}}  // namespace raft{namespace tcp{

#include <sys/timeb.h>

static STDN::mutex    s_mutexForCtime;

void lock_fprintfLocked(void)
{
	s_mutexForCtime.lock();
}


void unlock_fprintfLocked(void)
{
	s_mutexForCtime.unlock();
}

#ifdef _USE_LOG_FILES

static FILE* s_fpLogFile = NULL;
static FILE* s_fpErrorLogFile = NULL;
static int s_nLogFileShouldBeClosed = 0;
static int s_nErrorLogShouldBeClosed = 0;
static int s_nCleanupInited = 0;
static size_t s_unFileMaximumSize = 100000000; // 100 MB
static std::string s_strLogFileName;
static std::string s_strErrLogFileName;

static void CleanLogFilesInTheEndStatic(void)
{
	// s_mutexForCtime.lock();
	// because we are in the cleanup, let's skip locking

	if (s_fpLogFile && s_nLogFileShouldBeClosed) {fclose(s_fpLogFile);}
	s_fpLogFile = NULL;
	s_nLogFileShouldBeClosed = 0;

	if (s_fpErrorLogFile && s_nErrorLogShouldBeClosed) { fclose(s_fpErrorLogFile); }
	s_fpErrorLogFile = NULL;
	s_nErrorLogShouldBeClosed = 0;

	// s_mutexForCtime.unlock();
}


static FILE* OpenLogFileStatic(const char* a_cpcFileName,FILE** a_fppLogFile, int* a_pnLogFileWillBeClosed)
{
	FILE*& fpLogFile = *a_fppLogFile;
	int& nLogFileWillBeClosed = *a_pnLogFileWillBeClosed;

	s_mutexForCtime.lock();

	if (fpLogFile && nLogFileWillBeClosed) {
		fclose(fpLogFile);
	}
	fpLogFile = fopen(a_cpcFileName, "a+");

	if(fpLogFile){nLogFileWillBeClosed=1;}
	else{nLogFileWillBeClosed=0;}

	if(!s_nCleanupInited){
		atexit(&CleanLogFilesInTheEndStatic);
		s_nCleanupInited = 1;
	}

	s_mutexForCtime.unlock();

	return fpLogFile;
}


FILE* OpenRaftLogFile(const char* a_cpcFileName)
{
	s_strLogFileName = a_cpcFileName;
	return OpenLogFileStatic(a_cpcFileName,&s_fpLogFile,&s_nLogFileShouldBeClosed);
}


FILE* OpenRaftErrorLogFile(const char* a_cpcFileName)
{
	s_strErrLogFileName = a_cpcFileName;
	return OpenLogFileStatic(a_cpcFileName, &s_fpErrorLogFile, &s_nErrorLogShouldBeClosed);
}

void SetRaftLogFile(FILE* a_newLogFile)
{
	s_mutexForCtime.lock();
	s_fpLogFile = a_newLogFile;
	s_nLogFileShouldBeClosed = 0;
	s_mutexForCtime.unlock();
}


void SetRaftErrorLogFile(FILE* a_newLogFile)
{
	s_mutexForCtime.lock();
	s_fpErrorLogFile = a_newLogFile;
	s_nErrorLogShouldBeClosed = 0;
	s_mutexForCtime.unlock();
}


static void MoveFileAndResetContent(FILE** a_fppFile, const std::string& a_fileName, int a_nFileCanBeClosed)
{
	if(a_nFileCanBeClosed){
		FILE*& fpFile = *a_fppFile;
		struct stat fStat;
		if (!fstat(fileno(fpFile), &fStat)) {

            size_t unCurSize = (size_t)fStat.st_size;

            if (unCurSize>s_unFileMaximumSize) {
				std::string strBackFileName = a_fileName + ".back";
				remove(strBackFileName.c_str());
				fclose(fpFile);
				rename(a_fileName.c_str(), strBackFileName.c_str());
				fpFile = fopen(a_fileName.c_str(), "w");
			}

		} // if (!fstat(fileno(m_pFile), &fStat)) {
	}
}


void FlushLogFilesIfNonNull(void)
{
	if (s_fpLogFile) { /*printf("flushing!!!\n");*/ fflush(s_fpLogFile); }

	if ((s_fpLogFile!=s_fpErrorLogFile) && s_fpErrorLogFile) { fflush(s_fpErrorLogFile); }
	//if (s_fpErrorLogFile) { fflush(s_fpErrorLogFile); }
}


#endif  // #ifdef _USE_LOG_FILES

static int fprintfOnBothFilesIfNeededStatic(FILE* a_fpFile, const char* a_cpcFormat, va_list a_list)
{
    int nRet;
#ifdef _USE_LOG_FILES
    va_list aListTmp;
    va_copy(aListTmp, a_list);
#endif
    nRet = vfprintf(a_fpFile, a_cpcFormat, a_list);

#ifdef _USE_LOG_FILES
    if(s_fpLogFile && (a_fpFile==stdout)){
		static int snTimeToReset = 0;

        vfprintf(s_fpLogFile, a_cpcFormat, aListTmp);

		if (((++snTimeToReset) % 1000) == 0) {
			fflush(s_fpLogFile);
			if ((snTimeToReset % 1000) == 0) {
				MoveFileAndResetContent(&s_fpLogFile, s_strLogFileName, s_nLogFileShouldBeClosed);
			} // if (((++snTimeToReset) % 1000) == 0) {
		}
	}

    else if(s_fpLogFile && (a_fpFile==stderr)){
		static int snTimeToReset = 0;

        vfprintf(s_fpErrorLogFile, a_cpcFormat, aListTmp);

		if (((++snTimeToReset) % 1000) == 0) {
            fflush(s_fpErrorLogFile);
			if ((snTimeToReset % 1000) == 0) {
                MoveFileAndResetContent(&s_fpErrorLogFile, s_strErrLogFileName, s_nErrorLogShouldBeClosed);
			} // if (((++snTimeToReset) % 1000) == 0) {
		}

	}

    va_end(aListTmp);

#endif  // #ifdef _USE_LOG_FILES

	return nRet;
}



int fprintfOnBothFilesIfNeeded(FILE* a_fpFile, const char* a_cpcFormat, ...)
{
	int nRet;
	va_list aList;

	va_start(aList, a_cpcFormat);
	nRet = fprintfOnBothFilesIfNeededStatic(a_fpFile, a_cpcFormat, aList);
	va_end(aList);
	return nRet;
}


int fprintfWithTime(FILE* a_fpFile, const char* a_cpcFormat, ...)
{
	timeb	aCurrentTime;
	char* pcTimeline;
	int nRet;
	va_list aList;

	va_start(aList, a_cpcFormat);
	ftime(&aCurrentTime);
	pcTimeline = ctime(&(aCurrentTime.time));
	nRet = fprintfOnBothFilesIfNeeded(a_fpFile, "[%.19s.%.3hu %.4s] ", pcTimeline, aCurrentTime.millitm, &pcTimeline[20]);
	nRet += fprintfOnBothFilesIfNeededStatic(a_fpFile, a_cpcFormat, aList);

	va_end(aList);
	return nRet;
}

