#define WIN32_LEAN_AND_MEAN
#include "std.h"
#include "bbsockets.h"
#include <WinDNS.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <in6addr.h>
#include "../MultiLang/MultiLang.h"
#include "../gxruntime/gxutf8.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment (lib, "Urlmon.lib")
#pragma comment (lib, "Dnsapi.lib")
static bool socks_ok;
static WSADATA wsadata;
static int recv_timeout;
static int read_timeout;
static int accept_timeout;

static void close(SOCKET sock, int e) {
	if (e < 0) {
		int opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_DONTLINGER, (char*)&opt, sizeof(opt));
	}
	closesocket(sock);
}

class UDPStream;
class TCPStream;
class TCPServer;

static std::set<UDPStream*> udp_set;
static std::set<TCPStream*> tcp_set;
static std::set<TCPServer*> server_set;

class UDPStream : public bbStream {
public:
	UDPStream(SOCKET s);
	~UDPStream();

	int read(char* buff, int size);
	int write(const char* buff, int size);
	int avail();
	int eof();

	int recv();
	int send(const char* ip_str, int port);
	int getIP();
	int getPort();
	int getMsgIP();
	int getMsgPort();

private:
	SOCKET sock;
	std::vector<char> in_buf, out_buf;
	sockaddr_storage addr, in_addr, out_addr;
	socklen_t addr_len, in_addr_len, out_addr_len;
	int in_get, e;
};

UDPStream::UDPStream(SOCKET s) :sock(s), in_get(0), e(0), addr_len(0), in_addr_len(0), out_addr_len(0) {
	addr_len = sizeof(addr);
	if (getsockname(s, (sockaddr*)&addr, &addr_len) == SOCKET_ERROR) {
		e = -1;
	}
	in_addr = addr;
	in_addr_len = addr_len;
	out_addr = addr;
	out_addr_len = addr_len;
}

UDPStream::~UDPStream() {
	close(sock, e);
}

int UDPStream::read(char* buff, int size) {
	if (e) return 0;
	int n = in_buf.size() - in_get;
	if (n < size) size = n;
	memcpy(buff, &in_buf[in_get], size);
	in_get += size;
	return size;
}

int UDPStream::write(const char* buff, int size) {
	if (e) return 0;
	out_buf.insert(out_buf.end(), buff, buff + size);
	return size;
}

int UDPStream::avail() {
	if (e) return 0;
	return in_buf.size() - in_get;
}

int UDPStream::eof() {
	return e ? e : in_get == in_buf.size();
}

//fill buffer, return sender
int UDPStream::recv() {
	if (e) return 0;
	int tout;
	if (recv_timeout) tout = gx_runtime->getMilliSecs() + recv_timeout;
	for (;;) {
		int dt = 0;
		if (recv_timeout) {
			dt = tout - gx_runtime->getMilliSecs();
			if (dt < 0) dt = 0;
		}
		fd_set fd = { 1,sock };
		timeval tv = { dt / 1000,(dt % 1000) * 1000 };
		int n = ::select(0, &fd, 0, 0, &tv);
		if (!n) return 0;
		if (n != 1) { e = -1; return 0; }
		unsigned long sz = -1;
		if (ioctlsocket(sock, FIONREAD, &sz)) { e = -1; return 0; }
		in_buf.resize(sz); in_get = 0;
		in_addr_len = sizeof(in_addr);
		n = ::recvfrom(sock, (char*)in_buf.data(), sz, 0, (sockaddr*)&in_addr, &in_addr_len);
		if (n == SOCKET_ERROR) continue;	//{ e=-1;return 0; }
		in_buf.resize(n);
		return getMsgIP();
	}
	return 0;
}

//send, empty buffer
int UDPStream::send(const char* ip_str, int port) {
	if (e) return 0;

	addrinfo hints = {}, * result = nullptr;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	std::string service = std::to_string(port);
	if (getaddrinfo(ip_str, service.c_str(), &hints, &result) != 0) {
		return e = -1;
	}

	int sent = -1;

	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		sent = ::sendto(
			sock, out_buf.data(), (int)out_buf.size(), 0,
			ptr->ai_addr, (int)ptr->ai_addrlen
		);
		if (sent != SOCKET_ERROR) break;
	}

	freeaddrinfo(result);
	if (sent == SOCKET_ERROR) return e = -1;
	out_buf.clear();
	return sent;
}

int UDPStream::getIP() {
	if (addr.ss_family == AF_INET) {
		sockaddr_in* addr4 = (sockaddr_in*)&addr;
		return ntohl(addr4->sin_addr.S_un.S_addr);
	}
	else if (addr.ss_family == AF_INET6) {
		sockaddr_in6* addr6 = (sockaddr_in6*)&addr;
		if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
			uint32_t ip4 = *(uint32_t*)&addr6->sin6_addr.s6_addr[12];
			return ntohl(ip4);
		}
	}
	return 0;
}

int UDPStream::getPort() {
	if (addr.ss_family == AF_INET) {
		sockaddr_in* addr4 = (sockaddr_in*)&addr;
		return ntohs(addr4->sin_port);
	}
	else if (addr.ss_family == AF_INET6) {
		sockaddr_in6* addr6 = (sockaddr_in6*)&addr;
		return ntohs(addr6->sin6_port);
	}
	return 0;
}

int UDPStream::getMsgIP() {
	if (in_addr.ss_family == AF_INET) {
		sockaddr_in* addr4 = (sockaddr_in*)&in_addr;
		return ntohl(addr4->sin_addr.S_un.S_addr);
	}
	else if (in_addr.ss_family == AF_INET6) {
		sockaddr_in6* addr6 = (sockaddr_in6*)&in_addr;
		if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
			uint32_t ip4 = *(uint32_t*)&addr6->sin6_addr.s6_addr[12];
			return ntohl(ip4);
		}
	}
	return 0;
}

int UDPStream::getMsgPort() {
	if (in_addr.ss_family == AF_INET) {
		sockaddr_in* addr4 = (sockaddr_in*)&in_addr;
		return ntohs(addr4->sin_port);
	}
	else if (in_addr.ss_family == AF_INET6) {
		sockaddr_in6* addr6 = (sockaddr_in6*)&in_addr;
		return ntohs(addr6->sin6_port);
	}
	return 0;
}

class TCPStream : public bbStream {
public:
	TCPStream(SOCKET s, TCPServer* t);
	~TCPStream();

	int read(char* buff, int size);
	int write(const char* buff, int size);
	int avail();
	int eof();

	int getIP();
	int getPort();

private:
	SOCKET sock;
	TCPServer* server;
	int e, ip, port;
};

class TCPServer {
public:
	TCPServer(SOCKET S);
	~TCPServer();

	TCPStream* accept();

	void remove(TCPStream* s);

private:
	int e;
	SOCKET sock;
	std::set<TCPStream*> accepted_set;
};

TCPStream::TCPStream(SOCKET s, TCPServer* t) :sock(s), server(t), e(0), ip(0), port(0) {
	sockaddr_storage addr_storage = { 0 };
	int len = sizeof(addr_storage);
	if (getpeername(s, (sockaddr*)&addr_storage, &len) == 0) {
		if (addr_storage.ss_family == AF_INET) {
			sockaddr_in* addr4 = (sockaddr_in*)&addr_storage;
			ip = ntohl(addr4->sin_addr.S_un.S_addr);
			port = ntohs(addr4->sin_port);
		}
		else if (addr_storage.ss_family == AF_INET6) {
			sockaddr_in6* addr6 = (sockaddr_in6*)&addr_storage;
			if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
				ip = ntohl(*(uint32_t*)&addr6->sin6_addr.s6_addr[12]);
			}
			port = ntohs(addr6->sin6_port);
		}
	}
}

TCPStream::~TCPStream() {
	if (server) server->remove(this);
	close(sock, e);
}

int TCPStream::read(char* buff, int size) {
	if (e) return 0;
	char* b = buff, * l = buff + size;
	int tout;
	if (read_timeout) tout = gx_runtime->getMilliSecs() + read_timeout;
	while (b < l) {
		int dt = 0;
		if (read_timeout) {
			dt = tout - gx_runtime->getMilliSecs();
			if (dt < 0) dt = 0;
		}
		fd_set fd = { 1,sock };
		timeval tv = { dt / 1000,(dt % 1000) * 1000 };
		int n = ::select(0, &fd, 0, 0, &tv);
		if (n != 1) { e = -1; break; }
		n = ::recv(sock, b, l - b, 0);
		if (n == 0) { e = 1; break; }
		if (n == SOCKET_ERROR) { e = -1; break; }
		b += n;
	}
	return b - buff;
}

int TCPStream::write(const char* buff, int size) {
	if (e) return 0;
	int n = ::send(sock, buff, size, 0);
	if (n == SOCKET_ERROR) { e = -1; return 0; }
	return n;
}

int TCPStream::avail() {
	unsigned long t;
	int n = ::ioctlsocket(sock, FIONREAD, &t);
	if (n == SOCKET_ERROR) { e = -1; return 0; }
	return t;
}

int TCPStream::eof() {
	if (e) return e;
	fd_set fd = { 1,sock };
	timeval tv = { 0,0 };
	switch (::select(0, &fd, 0, 0, &tv)) {
	case 0:break;
	case 1:if (!avail()) e = 1; break;
	default:e = -1;
	}
	return e;
}

int TCPStream::getIP() {
	return ip;
}

int TCPStream::getPort() {
	return port;
}

TCPServer::TCPServer(SOCKET s) :sock(s), e(0) {
}

TCPServer::~TCPServer() {
	while (accepted_set.size()) delete* accepted_set.begin();
	close(sock, e);
}

TCPStream* TCPServer::accept() {
	if (e) return 0;
	sockaddr_storage addr;
	int addr_len = sizeof(addr);
	SOCKET t = ::accept(sock, (sockaddr*)&addr, &addr_len);
	if (t == INVALID_SOCKET) { e = -1; return 0; }
	TCPStream* s = new TCPStream(t, this);
	accepted_set.insert(s);
	return s;
}

void TCPServer::remove(TCPStream* s) {
	accepted_set.erase(s);
}

static inline void debugUDPStream(UDPStream* p, const char* function) {
	if (!udp_set.count(p)) {
		ErrorLog(function, MultiLang::udp_stream_not_exist);
	}
}

static inline void debugTCPStream(TCPStream* p, const char* function) {
	if (!tcp_set.count(p)) {
		ErrorLog(function, MultiLang::tcp_stream_not_exist);
	}
}

static inline void debugTCPServer(TCPServer* p, const char* function) {
	if (!server_set.count(p)) {
		ErrorLog(function, MultiLang::tcp_server_not_exist);
	}
}

static std::vector<int> host_ips;

int bbCountHostIPs(BBStr* host) {
	host_ips.clear();
	addrinfo hints = { 0 }, * result = nullptr;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(host->c_str(), nullptr, &hints, &result) != 0) {
		delete host;
		return 0;
	}

	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		if (ptr->ai_family == AF_INET) {
			sockaddr_in* addr = (sockaddr_in*)ptr->ai_addr;
			host_ips.push_back(ntohl(addr->sin_addr.s_addr));
		}
		else if (ptr->ai_family == AF_INET6) {
			sockaddr_in6* addr6 = (sockaddr_in6*)ptr->ai_addr;
			if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
				uint32_t ip4 = *(uint32_t*)&addr6->sin6_addr.s6_addr[12];
				host_ips.push_back(ntohl(ip4));
			}
		}
	}

	freeaddrinfo(result);
	delete host;
	return host_ips.size();
}

int bbHostIP(int index) {
	if (index<1 || index>host_ips.size()) {
		ErrorLog("HostIP", MultiLang::host_out_of_range);
	}
	return host_ips[index - 1];
}

UDPStream* bbCreateUDPStream(int port, int family) {
	if (!socks_ok) return 0;

	int af;
	int v6only = 0;

	if (family == 0) {
		af = AF_INET;
	}
	else if (family == 1) {
		af = AF_INET6;
		v6only = 1;
	}
	else if (family == 2) {
		af = AF_INET6;
		v6only = 0;
	}
	else {
		return 0;
	}

	SOCKET s = socket(af, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) return 0;

	if (af == AF_INET6) {
		if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only)) == SOCKET_ERROR) {
			closesocket(s);
			return 0;
		}
	}

	if (af == AF_INET) {
		sockaddr_in addr = { 0 };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
			closesocket(s);
			return 0;
		}
	}
	else {
		sockaddr_in6 addr6 = { 0 };
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(port);
		addr6.sin6_addr = in6addr_any;
		if (bind(s, (sockaddr*)&addr6, sizeof(addr6)) != 0) {
			closesocket(s);
			return 0;
		}
	}

	UDPStream* p = new UDPStream(s);
	udp_set.insert(p);
	return p;
}

void bbCloseUDPStream(UDPStream* p) {
	debugUDPStream(p, "CloseUDPStream");
	udp_set.erase(p);
	delete p;
}

int bbRecvUDPMsg(UDPStream* p) {
	debugUDPStream(p, "RecvUDPMsg");
	return p->recv();
}

void bbSendUDPMsg(UDPStream* p, BBStr* ip, int port) {
	debugUDPStream(p, "SendUDPMsg");
	p->send(ip->c_str(), port);
}

int bbUDPStreamIP(UDPStream* p) {
	debugUDPStream(p, "UDPStreamIP");
	return p->getIP();
}

int bbUDPStreamPort(UDPStream* p) {
	debugUDPStream(p, "UDPStreamPort");
	return p->getPort();
}

int bbUDPMsgIP(UDPStream* p) {
	debugUDPStream(p, "UDPMsgIP");
	return p->getMsgIP();
}

int bbUDPMsgPort(UDPStream* p) {
	debugUDPStream(p, "UDPMsgPort");
	return p->getMsgPort();
}

void bbUDPTimeouts(int rt) {
	recv_timeout = rt;
}

BBStr* bbDottedIP(int ip) {
	return new BBStr(
		itoa((ip >> 24) & 255) + "." + itoa((ip >> 16) & 255) + "." +
		itoa((ip >> 8) & 255) + "." + itoa(ip & 255));
}

static int findHostIP(const std::string& t) {
	addrinfo hints = { 0 }, * result = nullptr;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(t.c_str(), nullptr, &hints, &result) != 0) return -1;

	int ip = 0;
	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		if (ptr->ai_family == AF_INET) {
			sockaddr_in* addr = (sockaddr_in*)ptr->ai_addr;
			ip = ntohl(addr->sin_addr.s_addr);
			break;
		}
		else if (ptr->ai_family == AF_INET6) {
			sockaddr_in6* addr6 = (sockaddr_in6*)ptr->ai_addr;
			if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
				ip = ntohl(*(uint32_t*)&addr6->sin6_addr.s6_addr[12]);
				break;
			}
		}
	}
	freeaddrinfo(result);
	return ip;
}

TCPStream* bbOpenTCPStream(BBStr* server, int port, int local_port) {
	if (!socks_ok) {
		delete server;
		return 0;
	}

	addrinfo hints = { 0 }, * result = nullptr;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	std::string service = std::to_string(port);
	if (getaddrinfo(server->c_str(), service.c_str(), &hints, &result) != 0) {
		delete server;
		return 0;
	}

	SOCKET s = INVALID_SOCKET;
	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (s == INVALID_SOCKET) continue;

		if (local_port != 0) {
			sockaddr_storage local_addr = { 0 };
			if (ptr->ai_family == AF_INET) {
				sockaddr_in* local4 = (sockaddr_in*)&local_addr;
				local4->sin_family = AF_INET;
				local4->sin_port = htons(local_port);
				local4->sin_addr.s_addr = INADDR_ANY;
			}
			else {
				sockaddr_in6* local6 = (sockaddr_in6*)&local_addr;
				local6->sin6_family = AF_INET6;
				local6->sin6_port = htons(local_port);
				local6->sin6_addr = in6addr_any;
			}
			if (bind(s, (sockaddr*)&local_addr, ptr->ai_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) != 0) {
				closesocket(s);
				s = INVALID_SOCKET;
				continue;
			}
		}

		if (connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) break;
		closesocket(s);
		s = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	delete server;

	if (s != INVALID_SOCKET) {
		TCPStream* p = new TCPStream(s, 0);
		tcp_set.insert(p);
		return p;
	}
	return 0;
}

void bbCloseTCPStream(TCPStream* p) {
	debugTCPStream(p, "CloseTCPStream");
	tcp_set.erase(p);
	delete p;
}

TCPServer* bbCreateTCPServer(int port, int family) {
	if (!socks_ok) return 0;

	int af;
	int v6only = 0;

	if (family == 0) {
		af = AF_INET;
	}
	else if (family == 1) {
		af = AF_INET6;
		v6only = 1;
	}
	else if (family == 2) {
		af = AF_INET6;
		v6only = 0;
	}
	else {
		return 0;
	}

	SOCKET s = socket(af, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) return 0;

	if (af == AF_INET6) {
		if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only))) {
			closesocket(s);
			return 0;
		}
	}

	sockaddr_storage addr = { 0 };
	socklen_t addr_len;

	if (af == AF_INET) {
		sockaddr_in* addr4 = (sockaddr_in*)&addr;
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(port);
		addr4->sin_addr.s_addr = INADDR_ANY;
		addr_len = sizeof(sockaddr_in);
	}
	else {
		sockaddr_in6* addr6 = (sockaddr_in6*)&addr;
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons(port);
		addr6->sin6_addr = in6addr_any;
		addr_len = sizeof(sockaddr_in6);
	}

	if (bind(s, (sockaddr*)&addr, addr_len) != 0 || listen(s, SOMAXCONN) != 0) {
		closesocket(s);
		return 0;
	}

	TCPServer* p = new TCPServer(s);
	server_set.insert(p);
	return p;
}

void  bbCloseTCPServer(TCPServer* p) {
	debugTCPServer(p, "CloseTCPServer");
	server_set.erase(p);
	delete p;
}

TCPStream* bbAcceptTCPStream(TCPServer* server) {
	debugTCPServer(server, "AcceptTCPStream");
	if (!gx_runtime->idle()) RTEX(0);
	if (TCPStream* tcp = server->accept()) {
		tcp_set.insert(tcp);
		return tcp;
	}
	return 0;
}

int bbTCPStreamIP(TCPStream* p) {
	debugTCPStream(p, "TCPStreamIP");
	return p->getIP();
}

int bbTCPStreamPort(TCPStream* p) {
	debugTCPStream(p, "TCPStreamPort");
	return p->getPort();
}

void bbTCPTimeouts(int rt, int at) {
	read_timeout = rt;
	accept_timeout = at;
}

BBStr* bbParseDomainTXT(BBStr* txt, BBStr* name) {
	std::string s1 = txt->c_str();
	std::string s2 = name->c_str();
	std::string result = "";
	int n, a;
	if ((n = s1.find(s2 + "=")) != std::string::npos)
		result = s1.substr(n);
	if ((a = result.find(';')) != std::string::npos)
		result = result.substr(s2.length() + 1, a - s2.length() - 1);
	delete txt; delete name;
	return new BBStr(result);
}

BBStr* bbGetDomainTXT(BBStr* domain) {
	PDNS_RECORD pResult = NULL;
	DnsQuery_A(domain->c_str(), DNS_TYPE_TEXT, DNS_QUERY_BYPASS_CACHE, NULL, &pResult, NULL);
	std::string record = pResult->Data.TXT.pStringArray[0];
	delete domain;
	DnsRecordListFree(pResult, DnsFreeRecordListDeep);
	return new BBStr(record);
}

bool sockets_create() {
	socks_ok = WSAStartup(MAKEWORD(2, 2), &wsadata) == 0;
	recv_timeout = 0;
	read_timeout = 10000;
	accept_timeout = 0;
	return true;
}

bool sockets_destroy() {
	while (udp_set.size()) bbCloseUDPStream(*udp_set.begin());
	while (tcp_set.size()) bbCloseTCPStream(*tcp_set.begin());
	while (server_set.size()) bbCloseTCPServer(*server_set.begin());
	if (socks_ok) WSACleanup();
	return true;
}

int bbDownloadFile(BBStr* url, BBStr* file) {
	HRESULT result = URLDownloadToFileA(NULL, url->c_str(), file->c_str(), NULL, NULL);
	delete url; delete file;
	return result;
}

void sockets_link(void(*rtSym)(const char*, void*)) {
	rtSym("$DottedIP%IP", bbDottedIP);
	rtSym("%CountHostIPs$host_name", bbCountHostIPs);
	rtSym("%HostIP%host_index", bbHostIP);

	rtSym("%CreateUDPStream%port=0%family=0", bbCreateUDPStream);
	rtSym("CloseUDPStream%udp_stream", bbCloseUDPStream);
	rtSym("SendUDPMsg%udp_stream$dest_ip%dest_port=0", bbSendUDPMsg);
	rtSym("%RecvUDPMsg%udp_stream", bbRecvUDPMsg);
	rtSym("%UDPStreamIP%udp_stream", bbUDPStreamIP);
	rtSym("%UDPStreamPort%udp_stream", bbUDPStreamPort);
	rtSym("%UDPMsgIP%udp_stream", bbUDPMsgIP);
	rtSym("%UDPMsgPort%udp_stream", bbUDPMsgPort);
	rtSym("UDPTimeouts%recv_timeout", bbUDPTimeouts);

	rtSym("%OpenTCPStream$server%server_port%local_port=0", bbOpenTCPStream);
	rtSym("CloseTCPStream%tcp_stream", bbCloseTCPStream);
	rtSym("%CreateTCPServer%port%family=0", bbCreateTCPServer);
	rtSym("CloseTCPServer%tcp_server", bbCloseTCPServer);
	rtSym("%AcceptTCPStream%tcp_server", bbAcceptTCPStream);
	rtSym("%TCPStreamIP%tcp_stream", bbTCPStreamIP);
	rtSym("%TCPStreamPort%tcp_stream", bbTCPStreamPort);
	rtSym("TCPTimeouts%read_millis%accept_millis", bbTCPTimeouts);

	rtSym("$GetDomainTXT$domain", bbGetDomainTXT);
	rtSym("$ParseDomainTXT$txt$name", bbParseDomainTXT);

	rtSym("%DownloadFile$url$file", bbDownloadFile);
}