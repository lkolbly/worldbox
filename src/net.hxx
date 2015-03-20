#ifndef NET_HXX
#define NET_HXX 1

#include<netinet/in.h>
#include<vector>
#include<include/v8.h>

#include "net/messages.pb.h"

class NetLocationSubscription {
public:
  worldbox::GetEntityLocation _subscription;
  double _countdown;
};

/**
 * The network protocol is simple. Each message is preceeded by a 4-byte header.
 * The fields of this header are:
 * - 2 bytes: Size of message, in bytes.
 * - 2 bytes: The message type ID.
 */
class NetClient {
private:
  int _sockfd;
  struct sockaddr_in _addr;
  bool _is_open;
  std::vector<NetLocationSubscription*> _location_subscriptions;

public:
  NetClient(int sockfd, struct sockaddr_in addr);
  void update(double dt);
  void SendMessage(int type, std::string message);
  void SendLocationUpdate(Entity *entity, worldbox::GetEntityLocation msg);

  // Internal messaging...
  static void MessageReceiver(void *userdata, std::string channel, v8::Handle<v8::Value> message);
};

class NetServer {
private:
  int _server_sockfd;
  std::vector<NetClient*> _clients;

public:
  void startServer();
  void update(double dt);
  void SendMessageToAll(int msgtype, std::string msg);
};

#endif
