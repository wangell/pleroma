#include "netcode.h"
#include "../shared_src/protoloma.pb.h"
#include "concurrentqueue.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "pleroma.h"
#include "general_util.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <enet/enet.h>
#include <enet/types.h>
#include <immintrin.h>
#include <map>
#include <string>
#include <tuple>
#include <vector>

moodycamel::ConcurrentQueue<Msg> net_out_queue;
std::queue<Msg> net_in_queue;

moodycamel::ConcurrentQueue<Vat *> net_vats;

std::map<int, std::vector<Msg>> sort_queue;

struct PleromaNetwork {
  ENetHost *server;
  std::map<std::tuple<enet_uint32, enet_uint16>, ENetPeer *> peers;
  std::map<int, std::tuple<enet_uint32, enet_uint16>> node_host_map;
  u16 src_port;
} pnet;

std::string host32_to_string(u32 ip) {
  std::string ip_str;

  ip_str += std::to_string(((ip >> 0) & 0xFF));
  ip_str += ".";
  ip_str += std::to_string((ip >> 8) & 0xFF);
  ip_str += ".";
  ip_str += std::to_string((ip >> 16) & 0xFF);
  ip_str += ".";
  ip_str += std::to_string((ip >> 24) & 0xFF);

  return ip_str;
}

void announce_new_peer(enet_uint32 host, enet_uint16 port) {
  ENetAddress address;
  address.host = host;
  address.port = port;

  romabuf::PleromaMessage message;
  auto peer_msg = message.mutable_announce_peer();

  char ip_address[32];
  enet_address_get_host_ip(&address, ip_address, 32);

  peer_msg->set_address(std::string(ip_address));
  peer_msg->set_port(port);

  for (auto &[k, v] : pnet.peers) {
    ENetAddress peer_add;
    peer_add.host = std::get<0>(k);
    peer_add.port = std::get<1>(k);
    send_msg(peer_add, message);
  }
}

void assign_cluster_info_msg(enet_uint32 host, enet_uint16 port) {
  ENetAddress address;
  address.host = host;
  address.port = port;

  romabuf::PleromaMessage message;
  auto peer_msg = message.mutable_assign_cluster_info();

  char ip_address[32];
  enet_address_get_host_ip(&address, ip_address, 32);

  peer_msg->set_monad_entity_id(monad_ref->entity_id);
  peer_msg->set_monad_vat_id(monad_ref->vat_id);
  peer_msg->set_monad_node_id(monad_ref->node_id);

  this_pleroma_node.node_id++;
  peer_msg->set_node_id(this_pleroma_node.node_id);

  send_msg(address, message);
}

void net_loop() {
  ENetEvent event;
  romabuf::PleromaMessage message;

  /* Wait up to 1000 milliseconds for an event. */
  while (enet_host_service(pnet.server, &event, 0) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      printf("Got connection.\n");
      if (pnet.peers.find(std::make_tuple(event.peer->address.host,
                                          event.peer->address.port)) ==
          pnet.peers.end()) {
        printf("Connecting to...\n");
        u32 connect_back_port = event.data;

        connect_client(host32_to_string(event.peer->address.host), connect_back_port);
        printf("Connected, sending cluster info\n");

        assign_cluster_info_msg(event.peer->address.host, event.peer->address.port);

        announce_new_peer(event.peer->address.host, event.peer->address.port);
      }

      // send_msg(event.peer->address.host, message);
      /* Store any relevant client information here. */
      break;
    case ENET_EVENT_TYPE_RECEIVE:
      /* Clean up the packet now that we're done using it. */
      printf("got a packet!\n");
      on_receive_packet(&event);
      enet_packet_destroy(event.packet);

      break;

    case ENET_EVENT_TYPE_DISCONNECT:
      /* Reset the peer's client information. */
      event.peer->data = NULL;
    }
  }

  // Send all outgoing messages
  Msg out_mess;
  int n_received = 0;
  while (net_out_queue.try_dequeue(out_mess)) {
    // Sent from the void
    if (out_mess.node_id == -1) {
      continue;
    }
    if (out_mess.node_id == this_pleroma_node.node_id) {
      net_in_queue.push(out_mess);
    } else {
      send_node_msg(out_mess);
    }
    n_received++;

    if (n_received > 100)
      break;
  }

  // Put incoming messages into the correct mailboxes
  while (!net_in_queue.empty()) {
    auto msg_front = net_in_queue.front();
    sort_queue[msg_front.vat_id].push_back(msg_front);
    //printf("%d %d %d\n", msg_front.entity_id, msg_front.vat_id, msg_front.node_id);
    net_in_queue.pop();
  }
  Vat *vat_node;
  while (net_vats.try_dequeue(vat_node)) {
    if (sort_queue.find(vat_node->id) != sort_queue.end()) {
      for (auto zz : sort_queue[vat_node->id]) {
        vat_node->messages.push(zz);
      }
      sort_queue[vat_node->id].clear();
    }

    queue.enqueue(vat_node);
  }
}

void on_receive_packet(ENetEvent *event) {
  std::string buf =
      std::string((char *)event->packet->data, event->packet->dataLength);

  romabuf::PleromaMessage message;
  message.ParseFromString(buf);

  if (message.has_call()) {
    auto call = message.call();
    // MSMC
    Msg local_m;
    local_m.entity_id = call.entity_id();
    local_m.vat_id = call.vat_id();
    local_m.node_id = call.node_id();

    local_m.src_entity_id = call.src_entity_id();
    local_m.src_vat_id = call.src_vat_id();
    local_m.src_node_id = call.src_node_id();

    local_m.promise_id = call.promise_id();
    local_m.response = call.response();

    // for (auto val : message.pvalue_case()) {
    //}
    // local_m.values.push_back((ValueNode *)make_number(100));

    net_in_queue.push(local_m);
  } else {
    // announce peer
    printf("Got peer announcement!\n");
    auto apeer = message.announce_peer();
    ENetAddress address;
    enet_address_set_host(&address, apeer.address().c_str());
    address.port = apeer.port();
    if (pnet.peers.find(std::make_tuple(address.host, address.port)) == pnet.peers.end()) {

      // If it's not our own address
      if (address.host == pnet.server->address.host && address.port == pnet.server->address.port) {
        return;
      }

      printf("Connecting to new peer\n");
      connect_client(apeer.address(), apeer.port());
    }
  }
}

void init_network() {
  if (enet_initialize() != 0) {
    fprintf(stderr, "An error occurred while initializing ENet.\n");
    exit(EXIT_FAILURE);
  }
}

void send_packet(ENetPeer *peer, const char *buf, int buf_len) {
  ENetPacket *packet =
      enet_packet_create(buf, buf_len, ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
  enet_host_flush(pnet.server);
}

void send_msg(ENetAddress host, romabuf::PleromaMessage msg) {
  std::string buf = msg.SerializeAsString();

  send_packet(pnet.peers[std::make_tuple(host.host, host.port)], buf.c_str(),
              buf.length() + 1);
}

void send_node_msg(Msg m) {
  romabuf::PleromaMessage message;
  auto call = message.mutable_call();
  call->set_node_id(m.node_id);
  call->set_vat_id(m.vat_id);
  call->set_entity_id(m.entity_id);

  call->set_src_node_id(m.src_node_id);
  call->set_src_vat_id(m.src_vat_id);
  call->set_src_entity_id(m.src_entity_id);

  call->set_response(m.response);

  printf("CALL FUNC MSG %s\n", m.function_name.c_str());
  call->set_function_id(m.function_name);
  call->set_src_function_id(m.function_name);

  call->set_promise_id(m.promise_id);

  std::string buf = message.SerializeAsString();
  send_packet(pnet.peers[pnet.node_host_map[m.node_id]], buf.c_str(),
              buf.length() + 1);
}

void setup_server(std::string ip, u16 port) {
  ENetAddress address;
  enet_address_set_host(&address, ip.c_str());
  address.port = port;
  pnet.server = enet_host_create(&address, 32, 2, 0, 0);
  if (pnet.server == NULL) {
    fprintf(stderr,
            "An error occurred while trying to create an ENet server host.\n");
    exit(EXIT_FAILURE);
  }
  pnet.src_port = port;
}

void initial_connect() {
}

void connect_back() {
}

void handle_first_contact() {
}

void connect_client(std::string ip, u16 port) {
  ENetAddress address;
  enet_address_set_host(&address, ip.c_str());
  address.port = port;

  printf("Connecting to %s : %d\n", ip.c_str(), port);
  ENetPeer *peer = enet_host_connect(pnet.server, &address, 2, pnet.src_port);
  if (peer == NULL) {
    fprintf(stderr, "No available peers for initiating an ENet connection.\n");
    exit(EXIT_FAILURE);
  }

  ENetEvent event;
  if (enet_host_service(pnet.server, &event, 5000) > 0 &&
      event.type == ENET_EVENT_TYPE_CONNECT) {
    dbp(log_debug, "Connection succeeded.\n");
  } else {
    printf("Connection failed.\n");
    exit(EXIT_FAILURE);
  }

  while (enet_host_service(pnet.server, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
    printf("got event %d\n", event.type);
  }

    if (enet_host_service(pnet.server, &event, 5000) && event.type == ENET_EVENT_TYPE_RECEIVE) {
      printf("Assigned node ID + got Monad entity ref %d\n", event.type);
      std::string buf =
          std::string((char *)event.packet->data, event.packet->dataLength);

      romabuf::PleromaMessage message;
      message.ParseFromString(buf);
      assert(message.has_assign_cluster_info());

      pnet.node_host_map[0] = std::make_tuple(event.peer->address.host, event.peer->address.port);

      auto clusterinfo = message.assign_cluster_info();
      monad_ref = (EntityRefNode*)make_entity_ref(clusterinfo.monad_node_id(), clusterinfo.monad_vat_id(), clusterinfo.monad_entity_id());

      this_pleroma_node.node_id = clusterinfo.node_id();
      printf("Got the following info: our ID %d (%d %d %d)\n",
             this_pleroma_node.node_id, monad_ref->node_id, monad_ref->vat_id,
             monad_ref->entity_id);
    }

    pnet.peers[std::make_tuple(peer->address.host, peer->address.port)] = peer;
  }
