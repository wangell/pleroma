#include "netcode.h"
#include "../shared_src/protoloma.pb.h"
#include "../other_src/concurrentqueue.h"
#include "core/kernel.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include "other.h"
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

  // FIXME
  peer_msg->set_node_id(this_pleroma_node->node_id + 1);

  send_msg(address, message);
}

void net_loop() {
  ENetEvent event;
  romabuf::PleromaMessage message;

  // Add blocking wait here to reduce CPU
  while (enet_host_service(pnet.server, &event, 0) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      printf("handling\n");
      handle_connection(&event);
      break;
    case ENET_EVENT_TYPE_RECEIVE:
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
    if (out_mess.node_id == this_pleroma_node->node_id) {
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
    local_m.function_name = call.function_id();

    for (int i = 0; i < call.pvalues_size(); ++i) {
      auto pval = call.pvalues(i);
      if (pval.has_eref_val()) {
        auto eref = pval.eref_val();
        local_m.values.push_back((ValueNode *)make_entity_ref(eref.node_id(), eref.vat_id(), eref.entity_id()));
      }
      if (pval.has_num_val()) {
        auto num = pval.num_val();
        local_m.values.push_back((ValueNode *)make_number(num.value()));
      }
      if (pval.has_str_val()) {
        auto pval_str = pval.str_val();
        local_m.values.push_back((ValueNode *)make_string(pval_str.value()));
      }
    }

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
      connect_to_client(mk_netaddr(apeer.address(), apeer.port()));
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
  ENetPacket *packet = enet_packet_create(buf, buf_len, ENET_PACKET_FLAG_RELIABLE);
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

  call->set_function_id(m.function_name);
  call->set_src_function_id(m.function_name);

  call->set_promise_id(m.promise_id);

  for (auto &k : m.values) {
    auto pval = call->add_pvalues();
    if (k->type == AstNodeType::EntityRefNode) {
      EntityRefNode* node = (EntityRefNode*)k;
      romabuf::ERefVal e;
      auto blah = pval->mutable_eref_val();
      blah->set_node_id(node->node_id);
      blah->set_vat_id(node->vat_id);
      blah->set_entity_id(node->entity_id);
    }
    if (k->type == AstNodeType::NumberNode) {
      NumberNode* node = (NumberNode*)k;
      romabuf::NumVal n;
      auto blah = pval->mutable_num_val();
      blah->set_value(node->value);
    }
    if (k->type == AstNodeType::StringNode) {
      StringNode* node = (StringNode*)k;
      romabuf::StrVal n;
      auto blah = pval->mutable_str_val();
      blah->set_value(node->value);
    }
  }

  std::string buf = message.SerializeAsString();
  send_packet(pnet.peers[pnet.node_host_map[m.node_id]], buf.c_str(), buf.length() + 1);
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

ENetPeer *pconnect(ENetAddress address) {
  ENetPeer *peer = enet_host_connect(pnet.server, &address, 2, pnet.src_port);
  assert(peer);
  return peer;
}

bool connection_confirmed() {
  ENetEvent event;
  if (enet_host_service(pnet.server, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
    return true;
  } else {
    return false;
  }
}

void connect_to_client(ENetAddress address) {
  dbp(log_debug, "Connecting back to client");
  ENetPeer *peer = pconnect(address);

  if (connection_confirmed()) {
    dbp(log_debug, "Successfully connected to client");
  }

  pnet.peers[std::make_tuple(peer->address.host, peer->address.port)] = peer;
  // FIXME
  pnet.node_host_map[1] = std::make_tuple(peer->address.host, peer->address.port);
}

void connect_to_cluster(ENetAddress address) {
  ENetPeer* peer = pconnect(address);

  // Confirm outgoing connection
  if (connection_confirmed()) {
    dbp(log_debug, "Outgoing connection succeeded.");
  } else {
    throw PleromaException("Failed to connect to remote cluster.");
  }

  if (connection_confirmed()) {
    dbp(log_debug, "Incoming connection succeeded.");
  } else {
    throw PleromaException("Remote cluster failed to connect to this local host.");
  }

  romabuf::PleromaMessage message;
  auto host_info = message.mutable_host_info();
  host_info->set_port(pnet.src_port);
  // This should be assigned by cluster
  auto ndadd = host_info->mutable_nodeman_addr();

  ndadd->set_node_id(this_pleroma_node->nodeman_addr.node_id);
  ndadd->set_vat_id(this_pleroma_node->nodeman_addr.vat_id);
  ndadd->set_entity_id(this_pleroma_node->nodeman_addr.entity_id);

  //FIXME
  host_info->set_node_id(0);
  host_info->set_address("blah");

  auto res = host_info->add_resources();

  for (auto &k : this_pleroma_node->resources) {
    res->append(k);
  }

  std::string buf = message.SerializeAsString();
  send_packet(peer, buf.c_str(), buf.length() + 1);

  ENetEvent event;
  // Receive our cluster number/Monad info
  if (enet_host_service(pnet.server, &event, 5000) && event.type == ENET_EVENT_TYPE_RECEIVE) {
    std::string buf = std::string((char *)event.packet->data, event.packet->dataLength);

    romabuf::PleromaMessage message;
    message.ParseFromString(buf);
    assert(message.has_assign_cluster_info());

    // FIXME
    pnet.node_host_map[0] = std::make_tuple(event.peer->address.host, event.peer->address.port);

    auto clusterinfo = message.assign_cluster_info();
    monad_ref = (EntityRefNode*)make_entity_ref(clusterinfo.monad_node_id(), clusterinfo.monad_vat_id(), clusterinfo.monad_entity_id());

    this_pleroma_node->node_id = clusterinfo.node_id();
    printf("Got the following info: our ID %d (%d %d %d)\n", this_pleroma_node->node_id, monad_ref->node_id, monad_ref->vat_id, monad_ref->entity_id);
  }

  pnet.peers[std::make_tuple(peer->address.host, peer->address.port)] = peer;
}

void handle_connection(ENetEvent* event) {
    dbp(log_debug, "New node connecting...");
    u32 connect_back_port = event->data;

    connect_to_client(mk_netaddr(host32_to_string(event->peer->address.host), connect_back_port));

    dbp(log_debug, "Sending cluster info...");
    // TODO insert cluster info into kernel list here + sync over Raft
    ENetEvent event_in;
    if (enet_host_service(pnet.server, &event_in, 5000) && event_in.type == ENET_EVENT_TYPE_RECEIVE) {
      std::string buf = std::string((char *)event_in.packet->data, event_in.packet->dataLength);
      std::string((char *)event_in.packet->data, event_in.packet->dataLength);

      romabuf::PleromaMessage message;
      message.ParseFromString(buf);
      assert(message.has_host_info());

      PleromaNode *new_node = new PleromaNode;
      for (auto &k : message.host_info().resources()) {
        new_node->resources.push_back(k);
      }

      // FIXME
      new_node->node_id = this_pleroma_node->node_id + 1;
      new_node->nodeman_addr.node_id = this_pleroma_node->node_id + 1;
      new_node->nodeman_addr.vat_id = message.host_info().nodeman_addr().vat_id();
      new_node->nodeman_addr.entity_id = message.host_info().nodeman_addr().entity_id();
      printf("Received nodeman addr: %d %d %d\n", new_node->nodeman_addr.node_id, new_node->nodeman_addr.vat_id, new_node->nodeman_addr.entity_id);
      add_new_pnode(new_node);
    }

      assign_cluster_info_msg(event->peer->address.host,
                              event->peer->address.port);

      // announce_new_peer(event->peer->address.host,
      // event->peer->address.port);
    }

ENetAddress mk_netaddr(std::string ip, u16 port) {
  ENetAddress address;
  enet_address_set_host(&address, ip.c_str());
  address.port = port;
  return address;
}
