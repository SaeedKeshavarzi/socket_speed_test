#include "pch.h"

#include "util/sockio.h"
#include "util/resettable_event.h"
#include "speed_test_config.hpp"

#define MAX_UDP_PACKET_SIZE 0xffff
#define CONFIG_FILE_ADDRESS "./../speed_test_config.cfg"

using namespace std::chrono;
using namespace std::literals::chrono_literals;

static speed_test_config_t config{ "Test Config" };
static std::thread * threads;
static std::size_t n_connection{ 0 };

static bool keep_on{ true };
static std::atomic_llong pack_cnt{ 0 };
static std::atomic_int connection_cnt{ 0 };
static resettable_event<false> ready{ false };
static resettable_event<false> start{ false };

static socket_t * connection{ nullptr };

static void tx_start();
static void rx_udp_start();
static void rx_tcp_start();

static void tx_core(std::size_t server_id, std::size_t client_id, std::size_t port_id, std::size_t link_id);
static void rx_core(socket_t & link, std::size_t server_id, std::size_t client_id, std::size_t port_id, std::size_t link_id);
static bool get_client_id(const socket_t & link, const std::size_t server_id, std::size_t & client_id);

int main()
{
	INIT();

	config.scan(config.read_file(CONFIG_FILE_ADDRESS));
	config.write_file(CONFIG_FILE_ADDRESS);

	assert((config.protocol() == speed_test_config_t::ip_protocol_t::tcp) || (config.pack_len() <= MAX_UDP_PACKET_SIZE));

	For(srv_id, config.server_count())
	{
		For(cli_id, config.server(srv_id).client_count())
			n_connection += config.server(srv_id).client(cli_id).port_count();
	}

	printf("\nestablishing connection... \n");

	threads = new std::thread[n_connection];
	connection = new socket_t[n_connection];

	if (config.mode() == speed_test_config_t::test_mode_t::tx)
		tx_start();
	else if (config.mode() == speed_test_config_t::test_mode_t::rx)
	{
		if (config.protocol() == speed_test_config_t::ip_protocol_t::udp)
			rx_udp_start();
		else
			rx_tcp_start();
	}
	else
		throw new std::invalid_argument("Invalid Test Mode!");

	if (keep_on)
	{
		ready.wait();
		printf("connection established... \n");
	}

	std::thread wait_for_user_thread = std::thread([](bool & keep_on)
	{
		if (keep_on)
			std::this_thread::sleep_for(1500ms);

		printf("press enter to exit... \n\n");
		GET_CHAR();
		keep_on = false;
	}, std::ref(keep_on));

	auto start_time = high_resolution_clock::now();
	start.set();

	while (keep_on)
	{
		std::this_thread::sleep_for(2500ms);
		if (!keep_on)
			break;

		const auto now = high_resolution_clock::now();
		const auto ms = duration_cast<milliseconds>(now - start_time).count();
		start_time = now;

		printf("%3.3lf Mbps \n\n", (pack_cnt.exchange(0ll) * config.pack_len() * 8.) / (ms * 1000.));
	}

	wait_for_user_thread.join();

	For(con_id, n_connection)
		connection[con_id].close();

	For(con_id, n_connection)
	{
		if (threads[con_id].joinable())
			threads[con_id].join();
	}

	delete[] threads;
	delete[] connection;

	FINISH(0);
}

static void tx_start()
{
	std::size_t con_id{ 0 };

	For(srv_id, config.server_count())
	{
		For(cli_id, config.server(srv_id).client_count())
		{
			For(prt_id, config.server(srv_id).client(cli_id).port_count())
			{
				threads[con_id] = std::thread(tx_core, srv_id, cli_id, prt_id, con_id);
				++con_id;
			}
		}
	}
}

static void rx_udp_start()
{
	std::size_t con_id{ 0 };

	For(srv_id, config.server_count())
	{
		For(cli_id, config.server(srv_id).client_count())
		{
			For(prt_id, config.server(srv_id).client(cli_id).port_count())
			{
				threads[con_id] = std::thread(rx_core, std::ref(connection[con_id]), srv_id, cli_id, prt_id, con_id);
				++con_id;
			}
		}
	}
}

static void rx_tcp_start()
{
	std::size_t cnt{ 0 };

	For(srv_id, config.server_count())
	{
		tcp_server_t server;
		int ret = server.create(config.server(srv_id).ip_address(), (uint16_t)config.server(srv_id).port());
		if (ret != 0)
		{
			printf("%lluth server 'create' method failed on (%s %d) (Error Code: %d) \n",
				srv_id + 1, config.server(srv_id).ip_address().c_str(), config.server(srv_id).port(), ret);

			keep_on = false;
			break;
		}

		For(cli_id, config.server(srv_id).client_count())
		{
			std::size_t port_id{ 0 };
			For(prt_id, config.server(srv_id).client(cli_id).port_count())
			{
				while (keep_on)
				{
					int ret = server.listen(connection[cnt]);
					if (ret != 0)
					{
						printf("%llu server 'listen' method failed on (%s %d) (Error Code: %d) \n",
							srv_id + 1, config.server(srv_id).ip_address().c_str(), config.server(srv_id).port(), ret);

						keep_on = false;
						break;
					}

					std::size_t client_id;
					if (!get_client_id(connection[cnt], srv_id, client_id))
					{
						printf("Unknown Client (%s, %d) \n", connection[cnt].pair_ip().c_str(), connection[cnt].pair_port());
						connection[cnt].close();
					}
					else
					{
						threads[cnt] = std::thread(rx_core, std::ref(connection[cnt]), srv_id, client_id, port_id, cnt);
						++cnt;
						++port_id;
						break;
					}
				}
			}
		}

		server.close();
	}
}

static void tx_core(std::size_t server_id, std::size_t client_id, std::size_t port_id, std::size_t link_id)
{
	char * packet = new char[8 + config.pack_len() - config.pack_len() % 8];
	int local_pack_cnt{ 0 };

	memset(packet, 0, config.pack_len());

	int ret;
	do {
		ret = connection[link_id].create(
			config.protocol() == speed_test_config_t::ip_protocol_t::tcp ? ip_protocol_t::tcp : ip_protocol_t::udp,
			config.server(server_id).client(client_id).ip_address());

		if (ret != 0)
		{
			printf("%lluth port of %lluth client of %lluth server: 'create' method failed! (Error Code: %d) \n",
				port_id + 1, client_id + 1, server_id + 1, ret);
		}
		else
		{
			uint16_t server_port{ (uint16_t)config.server(server_id).port() };
			if (config.protocol() == speed_test_config_t::ip_protocol_t::udp)
				server_port += (uint16_t)port_id;

			ret = connection[link_id].connect(config.server(server_id).ip_address(), server_port);
			if (ret != 0)
			{
				printf("%lluth port of %lluth client of %lluth server: 'connect' method failed! (Error Code: %d) \n",
					port_id + 1, client_id + 1, server_id + 1, ret);
			}
			else
			{
				if (connection_cnt.fetch_add(1) + 1 == (int)n_connection)
					ready.set();

				start.wait();
				break;
			}
		}

		std::this_thread::sleep_for(750ms);
	} while (true);

	while (keep_on)
	{
		*(int32_t*)packet = (local_pack_cnt > (1 << 30) ? local_pack_cnt = 0 : local_pack_cnt)++;

		ret = connection[link_id].send(packet, (int)config.pack_len());
		if (ret != 0)
		{
			printf("packet %d send failed! (Error Code: %d) \n", local_pack_cnt - 1, ret);
			keep_on = false;
			break;
		}

		pack_cnt.fetch_add(1ll);
	}

	delete[] packet;
}

static void rx_core(socket_t & link, std::size_t server_id, std::size_t client_id, std::size_t port_id, std::size_t link_id)
{
	(void)link_id;
	char * packet = new char[8 + config.pack_len() - config.pack_len() % 8];
	int32_t local_pack_cnt{ 0 };
	int ret{ 0 };

	if (config.protocol() == speed_test_config_t::ip_protocol_t::udp)
	{
		do {
			ret = link.create(ip_protocol_t::udp, config.server(server_id).ip_address(),
				(uint16_t)(config.server(server_id).port() + port_id));

			if (ret == 0)
				break;

			printf("%lluth port of %lluth client of %lluth server: 'create' method failed! (Error Code: %d) \n",
				port_id + 1, client_id + 1, server_id + 1, ret);

			std::this_thread::sleep_for(750ms);
		} while (true);
	}

	if (ret == 0)
	{
		if (connection_cnt.fetch_add(1) + 1 == n_connection)
			ready.set();

		start.wait();
	}

	while (keep_on)
	{
		ret = link.recv(packet, (int)config.pack_len());
		if (ret != 0)
		{
			printf("%lluth server, %lluth client, %lluth port receive failed! (Error Code: %d) \n",
				server_id + 1, client_id + 1, port_id + 1, ret);
			keep_on = false;
			break;
		}

		pack_cnt.fetch_add(1ll);

		if (config.protocol() == speed_test_config_t::ip_protocol_t::tcp)
		{
			if (*(int32_t*)packet != (local_pack_cnt > (1 << 30) ? local_pack_cnt = 0 : local_pack_cnt)++)
			{
				printf("%lluth server %dth packet corrupted! \n", server_id + 1, local_pack_cnt - 1);
				keep_on = false;
				break;
			}
		}
	}

	delete[] packet;
}

static bool get_client_id(const socket_t & link, const std::size_t server_id, std::size_t & client_id)
{
	For(cli_id, config.server(server_id).client_count())
	{
		if (link.pair_ip() == config.server(server_id).client(cli_id).ip_address())
		{
			client_id = cli_id;
			return true;
		}
	}

	return false;
}
