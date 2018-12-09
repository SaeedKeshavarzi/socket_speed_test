#ifndef _SOCKIO_H_
#define _SOCKIO_H_

#include <stdint.h>

enum class ip_protocol_t : int
{
	tcp = IPPROTO_TCP,
	udp = IPPROTO_UDP
};

class socket_t
{
public:
	int connect(const std::string & mine_ip, const uint16_t mine_port, const std::string & pair_ip, const uint16_t pair_port, const ip_protocol_t protocol);
	int connect(const std::string & pair_ip, const uint16_t pair_port, const ip_protocol_t protocol);

	int send(const char * packet, const int size);
	int recv(char * packet, const int size);
	int recv_any(char * packet, const int capacity, int * recvd_size = nullptr);

	std::string mine_ip();
	uint16_t mine_port();

	std::string pair_ip();
	uint16_t pair_port();

	int close();
	~socket_t();

private:
	SOCKET socket_id;
	sockaddr_in mine_address;
	sockaddr_in pair_address;

	friend class tcp_server_t;
};

class tcp_server_t
{
public:
	int create(const std::string & ip = "", const uint16_t port = 0);
	int listen(socket_t & client_socket);

	std::string ip();
	uint16_t port();

	int close();
	~tcp_server_t();

private:
	SOCKET server_id;
	sockaddr_in ip_address;
};

#endif // !_SOCKIO_H_
