#include "pch.h"

#include "util/sockio.h"
#include "util/resettable_event.h"
#include "speed_test_config.hpp"

#define MAX_UDP_PACKET_SIZE 0xffff
#define CONFIG_FILE_ADDRESS "./../speed_test_config.cfg"

using namespace std::chrono;

static speed_test_config_t config{ "Test Config" };
static std::thread * threads;
static std::size_t n_socket{ 0 };

static bool keep_on{ true };
static std::atomic_llong pack_cnt{ 0 };
static std::atomic_int connection_cnt{ 0 };
static resettable_event<false> ready{ false };
static resettable_event<false> start{ false };

static socket_t * connection{ nullptr };

static void tx_test(std::size_t server_ind, std::size_t client_ind, std::size_t port_ind, std::size_t link_ind);
static void rx_test(socket_t & link, std::size_t server_ind, std::size_t client_ind, std::size_t port_ind, std::size_t link_ind);
static bool get_client_ind(const socket_t & link, const std::size_t server_ind, std::size_t & client_ind);

int main()
{
	INIT();

	config.scan(config.read_file(CONFIG_FILE_ADDRESS));
	config.write_file(CONFIG_FILE_ADDRESS);

	assert((config.protocol() == speed_test_config_t::ip_protocol_t::tcp) || (config.pack_len() <= MAX_UDP_PACKET_SIZE));

	For(i, config.server_count())
	{
		For(j, config.server(i).client_count())
		{
			n_socket += config.server(i).client(j).port_count();
		}
	}

	printf("\nestablishing connection... \n");

	threads = new std::thread[n_socket];
	connection = new socket_t[n_socket];

	if (config.mode() == speed_test_config_t::test_mode_t::tx)
	{
		for (std::size_t i{ 0 }, cnt{ 0 }; i < config.server_count(); ++i)
		{
			For(j, config.server(i).client_count())
			{
				For(k, config.server(i).client(j).port_count())
				{
					threads[cnt] = std::thread(tx_test, i, j, k, cnt);
					++cnt;
				}
			}
		}
	}
	else if (config.mode() == speed_test_config_t::test_mode_t::rx)
	{
		if (config.protocol() == speed_test_config_t::ip_protocol_t::udp)
		{
			for (std::size_t i{ 0 }, cnt{ 0 }; i < config.server_count(); ++i)
			{
				For(j, config.server(i).client_count())
				{
					For(k, config.server(i).client(j).port_count())
					{
						threads[cnt] = std::thread(rx_test, std::ref(connection[cnt]), i, j, k, cnt);
						++cnt;
					}
				}
			}
		}
		else
		{
			for (std::size_t i{ 0 }, cnt{ 0 }; i < config.server_count(); ++i)
			{
				tcp_server_t server;
				int ret = server.create(config.server(i).ip_address(), (uint16_t)config.server(i).port());
				if (ret != 0)
				{
					printf("%lluth server 'create' method failed on (%s %d) (Error Code: %d) \n",
						i + 1, config.server(i).ip_address().c_str(), config.server(i).port(), ret);

					keep_on = false;
					break;
				}

				for (std::size_t j = 0; keep_on && (j < config.server(i).client_count()); ++j)
				{
					std::size_t port_ind{ 0 };
					for (std::size_t k = 0; keep_on && (k < config.server(i).client(j).port_count()); ++k)
					{
						while (keep_on)
						{
							int ret = server.listen(connection[cnt], (int)n_socket);
							if (ret != 0)
							{
								printf("%llu server 'listen' method failed on (%s %d) (Error Code: %d) \n",
									i + 1, config.server(i).ip_address().c_str(), config.server(i).port(), ret);

								keep_on = false;
								break;
							}

							std::size_t client_ind;
							if (!get_client_ind(connection[cnt], i, client_ind))
							{
								printf("Unknown Client (%s, %d) \n", connection[cnt].pair_ip().c_str(), connection[cnt].pair_port());
								connection[cnt].close();
							}
							else
							{
								threads[cnt] = std::thread(rx_test, std::ref(connection[cnt]), i, client_ind, port_ind, cnt);
								++cnt;
								++port_ind;
								break;
							}
						}
					}
				}

				server.close();
			}
		}
	}
	else
	{
		throw new std::invalid_argument("Invalid Test Mode!");
	}

	if (keep_on)
	{
		ready.wait();
		printf("connection established... \n");
	}

	std::thread wait_for_user_thread = std::thread([](bool &trigger) {
		if (trigger)
		{
			std::this_thread::sleep_for(milliseconds(1500));
		}

		printf("press enter to exit... \n\n");
		GET_CHAR();
		trigger = false;
	}, std::ref(keep_on));

	time_point<high_resolution_clock> start_time{ high_resolution_clock::now() };
	start.set();

	while (keep_on)
	{
		std::this_thread::sleep_for(milliseconds(2500));
		if (!keep_on)
		{
			break;
		}

		const auto && now = high_resolution_clock::now();
		const auto && ms = duration_cast<milliseconds>(now - start_time).count();
		start_time = now;

		printf("%3.3lf Mbps \n\n", (pack_cnt.exchange(0ll) * config.pack_len() * 8.) / (ms * 1000.));
	}

	wait_for_user_thread.join();

	For(i, n_socket)
	{
		connection[i].close();
	}

	For(i, n_socket)
	{
		if (threads[i].joinable())
		{
			threads[i].join();
		}
	}

	delete[] threads;
	delete[] connection;

	FINISH(0);
}

static void tx_test(std::size_t server_ind, std::size_t client_ind, std::size_t port_ind, std::size_t link_ind)
{
	char * packet = new char[8 + config.pack_len() - config.pack_len() % 8];
	int local_pack_cnt{ 0 };

	memset(packet, 0, config.pack_len());

	int ret;
	do {
		ret = connection[link_ind].create(
			config.protocol() == speed_test_config_t::ip_protocol_t::tcp ? ip_protocol_t::tcp : ip_protocol_t::udp,
			config.server(server_ind).client(client_ind).ip_address());

		if (ret != 0)
		{
			printf("%lluth port of %lluth client of %lluth server: 'create' method failed! (Error Code: %d) \n",
				port_ind + 1, client_ind + 1, server_ind + 1, ret);
		}
		else
		{
			ret = connection[link_ind].connect(config.server(server_ind).ip_address(), (uint16_t)config.server(server_ind).port());

			if (ret != 0)
			{
				printf("%lluth port of %lluth client of %lluth server: 'connect' method failed! (Error Code: %d) \n",
					port_ind + 1, client_ind + 1, server_ind + 1, ret);
			}
			else
			{
				if (connection_cnt.fetch_add(1) + 1 == (int)n_socket)
				{
					ready.set();
				}

				start.wait();
				break;
			}
		}

		std::this_thread::sleep_for(milliseconds(750));
	} while (true);

	while (keep_on)
	{
		*(int32_t*)packet = (local_pack_cnt > (1 << 30) ? local_pack_cnt = 0 : local_pack_cnt)++;

		ret = connection[link_ind].send(packet, (int)config.pack_len());
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

static void rx_test(socket_t & link, std::size_t server_ind, std::size_t client_ind, std::size_t port_ind, std::size_t link_ind)
{
	(void)link_ind;
	char * packet = new char[8 + config.pack_len() - config.pack_len() % 8];
	int32_t local_pack_cnt{ 0 };
	int ret{ 0 };

	if (config.protocol() == speed_test_config_t::ip_protocol_t::udp)
	{
		do {
			ret = link.create(ip_protocol_t::udp, config.server(server_ind).ip_address(),
				config.server(server_ind).port());

			if (ret != 0)
			{
				printf("%lluth port of %lluth client of %lluth server: 'create' method failed! (Error Code: %d) \n",
					port_ind + 1, client_ind + 1, server_ind + 1, ret);

				std::this_thread::sleep_for(milliseconds(750));
			}
			else
			{
				break;
			}
		} while (true);
	}

	if (ret == 0)
	{
		if (connection_cnt.fetch_add(1) + 1 == n_socket)
		{
			ready.set();
		}

		start.wait();
	}

	while (keep_on)
	{
		ret = link.recv(packet, (int)config.pack_len());
		if (ret != 0)
		{
			printf("%lluth server, %lluth client, %lluth port receive failed! (Error Code: %d) \n",
				server_ind + 1, client_ind + 1, port_ind + 1, ret);
			keep_on = false;
			break;
		}

		pack_cnt.fetch_add(1ll);

		if (config.protocol() == speed_test_config_t::ip_protocol_t::tcp)
		{
			if (*(int32_t*)packet != (local_pack_cnt > (1 << 30) ? local_pack_cnt = 0 : local_pack_cnt)++)
			{
				printf("%lluth server %dth packet corrupted! \n", server_ind + 1, local_pack_cnt - 1);
				keep_on = false;
				break;
			}
		}
	}

	delete[] packet;
}

static bool get_client_ind(const socket_t & link, const std::size_t server_ind, std::size_t & client_ind)
{
	For(i, config.server(server_ind).client_count())
	{
		if (link.pair_ip() == config.server(server_ind).client(i).ip_address())
		{
			client_ind = i;
			return true;
		}
	}

	return false;
}
