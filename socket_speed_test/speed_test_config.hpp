#ifndef _SPEED_TEST_CONFIG_HPP_
#define _SPEED_TEST_CONFIG_HPP_

#include "util/setting_t.hpp"

class client_config_t : public group_t
{
private:
	scalar_t<std::string> ip_address_{ "IP Address" };
	scalar_t<std::size_t> port_count_{ "Port Count" };

public:
	std::size_t size() const
	{
		return 2;
	}

	const setting_t & operator()(std::size_t index) const
	{
		switch (index)
		{
		case 0:
			return ip_address_;
		case 1:
			return port_count_;
		default:
			throw new std::invalid_argument("Invalid index!");
		}
	}

	inline std::string ip_address() const { return ip_address_(); }
	inline void ip_address(const std::string & _ip_address) { ip_address_() = _ip_address; }

	inline std::size_t port_count() const { return port_count_(); }
	inline void port_count(const std::size_t & _port_count) { port_count_() = _port_count; }
};

class server_config_t : public group_t
{
private:
	scalar_t<std::string> ip_address_{ "IP Address" };
	scalar_t<int> port_{ "Port Number" };
	vector_t<client_config_t> client_{ "Client" };

public:
	std::size_t size() const
	{
		return 3;
	}

	const setting_t & operator()(std::size_t index) const
	{
		switch (index)
		{
		case 0:
			return ip_address_;
		case 1:
			return port_;
		case 2:
			return client_;
		default:
			throw new std::invalid_argument("Invalid index!");
		}
	}

	inline std::string ip_address() const { return ip_address_(); }
	inline void ip_address(const std::string _server_address) { ip_address_() = _server_address; }

	inline int port() const { return port_(); }
	inline void port(const int _server_port) { port_() = _server_port; }

	inline std::size_t client_count() const { return client_.size(); }

	inline client_config_t& client(std::size_t index) { return client_(index); }
	inline const client_config_t& client(std::size_t index) const { return client_(index); }
};

class speed_test_config_t : public group_t
{
public:
	speed_test_config_t(const std::string & _label = "Speed Test Config") : group_t(_label) {  }

	enum class ip_protocol_t : int
	{
		tcp = 0,
		udp
	};

	enum class test_mode_t : int
	{
		tx = 0,
		rx
	};

	std::size_t size() const
	{
		return 4;
	}

	const setting_t & operator()(std::size_t index) const
	{
		switch (index)
		{
		case 0:
			return protocol_;
		case 1:
			return pack_len_;
		case 2:
			return mode_;
		case 3:
			return server_;
		default:
			throw new std::invalid_argument("Invalid index!");
		}
	}

	inline ip_protocol_t protocol() const { return (ip_protocol_t)protocol_(); }
	inline void protocol(ip_protocol_t _protocol) { protocol_() = (int)_protocol; }

	inline int pack_len() const { return pack_len_(); }
	inline void pack_len(int _pack_len) { pack_len_() = _pack_len; }

	inline test_mode_t mode() const { return (test_mode_t)mode_(); }
	inline void mode(test_mode_t _mode) { mode_() = (int)_mode; }

	inline std::size_t server_count() const { return server_.size(); }

	inline server_config_t& server(const std::size_t & index) { return server_(index); }
	inline const server_config_t& server(const std::size_t & index) const { return server_(index); }

private:
	scalar_t<int> protocol_{ "Protocol (0: TCP, 1: UDP)" };
	scalar_t<int> pack_len_{ "Packet Length" };
	scalar_t<int> mode_{ "Mode (0: Tx, 1: Rx)" };
	vector_t<server_config_t> server_{ "Server" };;
};

#endif // !_SPEED_TEST_CONFIG_HPP_
