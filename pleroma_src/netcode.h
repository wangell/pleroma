#pragma once

#include "../shared_src/protoloma.pb.h"
#include <enet/enet.h>
#include <string>

void init_network();
void net_loop();
void setup_server();
void connect_client(ENetAddress);
ENetAddress get_address(std::string);

void send_msg(enet_uint32 host, romabuf::PleromaMessage msg);

void on_receive_packet(ENetEvent *event);
