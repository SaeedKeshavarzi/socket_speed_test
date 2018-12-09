#include "pch.h"

#include "util/sockio.h"
#include "util/resettable_event.h"
#include "speed_test_config.hpp"

#define CONFIG_FILE_ADDRESS "./../speed_test_config.cfg"

speed_test_config_t config{ "Test Config" };
std::thread * threads;
std::size_t n_socket{ 0 };

bool keep_on{ true };
std::atomic_llong pack_cnt{ 0 };

tcp_server_t * server{ nullptr };
socket_t * connection{ nullptr };

std::atomic_int connection_cnt{ 0 };
manual_reset_event ready{ false };
manual_reset_event start{ false };

void tx_test(int server_ind, int client_ind, int port_ind)
{
	char * packet = new char[config.pack_len()];
	int local_pack_cnt{ 0 };

	Forn(i, 4, config.pack_len())
	{
		packet[i] = (char)(i % 125);
	}

	socket_t tcp_client;
	auto ret = tcp_client.connect(
		config.server(server_ind).client(client_ind).ip_address(),
		config.server(server_ind).client(client_ind).port(port_ind),
		config.server(server_ind).ip_address(),
		config.server(server_ind).port(), ip_protocol_t::tcp);

	if (ret != 0)
	{
		printf("%dth port of %dth client of %dth server: 'connect' method failed! (Error Code: %d) \n",
			port_ind + 1, client_ind + 1, server_ind + 1, ret);

		keep_on = false;
	}
	else
	{
		if (connection_cnt.fetch_add(1) + 1 == n_socket)
		{
			ready.set();
		}

		start.wait();
	}

	while (keep_on)
	{
		*(int*)packet = (local_pack_cnt > (1 << 30) ? local_pack_cnt = 0 : local_pack_cnt++);
		pack_cnt.fetch_add(1ll);

		ret = tcp_client.send(packet, config.pack_len());
		if (ret != 0)
		{
			printf("packet %d send failed! (Error Code: %d) \n", *(int*)packet, ret);
			keep_on = false;
			break;
		}
	}

	tcp_client.close();
	delete[] packet;
}

void rx_test(int server_ind, int socket_ind)
{
	int ret;
	int local_pack_cnt{ 0 };
	char * packet = new char[config.pack_len()];
	char * ref_packet = new char[config.pack_len()];

	Forn(i, 4, config.pack_len())
	{
		ref_packet[i] = (char)(i % 125);
	}

	start.wait();

	while (keep_on)
	{
		*(int*)ref_packet = (local_pack_cnt > (1 << 30) ? local_pack_cnt = 0 : local_pack_cnt++);
		pack_cnt.fetch_add(1ll);

		ret = connection[socket_ind].recv(packet, config.pack_len());
		if (ret != 0)
		{
			printf("%dth server %dth packet receive failed! (Error Code: %d) \n", server_ind + 1, *(int*)ref_packet, ret);
			keep_on = false;
			break;
		}

		if (memcmp(packet, ref_packet, config.pack_len()) != 0)
		{
			printf("%dth server %dth packet corrupted! \n", server_ind + 1, *(int*)ref_packet);
			keep_on = false;
			break;
		}
	}

	connection[socket_ind].close();

	delete[] packet;
	delete[] ref_packet;
}

int main()
{
	INIT();

	config.scan(config.read_file(CONFIG_FILE_ADDRESS));
	config.write_file(CONFIG_FILE_ADDRESS);

	printf("\nestablishing connection... \n");

	For(i, config.server_count())
	{
		For(j, config.server(i).client_count())
		{
			n_socket += config.server(i).client(j).port_count();
		}
	}

	threads = new std::thread[n_socket];

	if (config.mode() == speed_test_config_t::test_mode_t::tx)
	{
		for (int i{ 0 }, cnt{ 0 }; i < config.server_count(); ++i)
		{
			For(j, config.server(i).client_count())
			{
				For(k, config.server(i).client(j).port_count())
				{
					threads[cnt] = std::thread(tx_test, i, j, k);
					++cnt;
				}
			}
		}
	}
	else if (config.mode() == speed_test_config_t::test_mode_t::rx)
	{
		server = new tcp_server_t[config.server_count()];
		connection = new socket_t[n_socket];

		for (int i{ 0 }, cnt{ 0 }; i < config.server_count(); ++i)
		{
			int ret = server[i].create(config.server(i).ip_address(), config.server(i).port());
			if (ret != 0)
			{
				printf("%dth server 'create' method failed on (%s %d) (Error Code: %d) \n",
					i + 1, config.server(i).ip_address().c_str(), config.server(i).port(), ret);

				keep_on = false;
				break;
			}

			For(j, config.server(i).client_count())
			{
				For(k, config.server(i).client(j).port_count())
				{
					int ret = server[i].listen(connection[cnt]);
					if (ret != 0)
					{
						printf("%d server 'listen' method failed on (%s %d) (Error Code: %d) \n",
							i + 1, config.server(i).ip_address().c_str(), config.server(i).port(), ret);

						keep_on = false;
						break;
					}

					threads[cnt] = std::thread(rx_test, i, cnt);
					++cnt;
				}
			}
		}

		ready.set();
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
			std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		}

		printf("press enter to exit... \n\n");
		GET_CHAR();
		trigger = false;
	}, std::ref(keep_on));

	std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> start_time{ std::chrono::high_resolution_clock::now() };
	start.set();

	while (keep_on)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		if (!keep_on)
		{
			break;
		}

		const auto && last_start_time = start_time.exchange(std::chrono::high_resolution_clock::now());
		const auto && ms = std::chrono::duration_cast<std::chrono::milliseconds>(start_time.load() - last_start_time).count();

		printf("%3.3lf Mbps \n\n", (pack_cnt.exchange(0ll) * config.pack_len() * 8.) / (ms * 1000.));
	}

	wait_for_user_thread.join();
	For(i, n_socket)
	{
		if (threads[i].joinable())
		{
			threads[i].join();
		}
	}

	delete[] threads;

	if (config.mode() == speed_test_config_t::test_mode_t::rx)
	{
		delete[] server;
		delete[] connection;
	}

	FINISH(0);
}
