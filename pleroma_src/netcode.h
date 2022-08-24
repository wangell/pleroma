#pragma once

#include "../shared_src/protoloma.pb.h"
#include "concurrentqueue.h"
#include "hylic_eval.h"
#include <enet/enet.h>
#include <queue>
#include <string>

#include "blockingconcurrentqueue.h"

extern moodycamel::ConcurrentQueue<Msg> net_out_queue;
extern moodycamel::BlockingConcurrentQueue<Vat *> queue;
extern std::queue<Msg> net_in_queue;

void init_network();
void net_loop();
void setup_server(std::string ip, u16 port);
void connect_client(std::string ip, u16 port);

void send_msg(ENetAddress host, romabuf::PleromaMessage msg);

void on_receive_packet(ENetEvent *event);
void send_node_msg(Msg m);

extern moodycamel::ConcurrentQueue<Vat *> net_vats;
