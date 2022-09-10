#pragma once

#include "../shared_src/protoloma.pb.h"
#include "../other_src/concurrentqueue.h"
#include "hylic_eval.h"
#include <enet/enet.h>
#include <queue>
#include <string>

#include "../other_src/blockingconcurrentqueue.h"

extern moodycamel::ConcurrentQueue<Msg> net_out_queue;
extern moodycamel::BlockingConcurrentQueue<Vat *> queue;
extern std::queue<Msg> net_in_queue;

void init_network();
void net_loop();
void setup_server(std::string ip, u16 port);
void connect_to_client(ENetAddress);
void connect_to_cluster(ENetAddress);

void send_msg(ENetAddress host, romabuf::PleromaMessage msg);

void on_receive_packet(ENetEvent *event);
void send_node_msg(Msg m);
void handle_connection(ENetEvent* event);
ENetAddress mk_netaddr(std::string ip, u16 port);

extern moodycamel::ConcurrentQueue<Vat *> net_vats;
