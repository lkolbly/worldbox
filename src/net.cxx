#include<sys/types.h> 
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<include/v8.h>
#include<bullet/btBulletDynamicsCommon.h>

#include "messaging.hxx"
#include "entity.hxx"
#include "net.hxx"
#include "util.hxx"

using namespace v8;

extern btDiscreteDynamicsWorld *physics_world;
void SpawnEntity(std::string config_str, Vec3 position, int64_t id, std::string init_cfg_str, Isolate *isolate);
Entity *GetEntityById(int64_t id);

void NetClient::SendMessage(int type, std::string message) {
  unsigned short msgsize=htons(message.length()), msgtype=htons(type);
  write(_sockfd, &msgsize, 2);
  write(_sockfd, &msgtype, 2);
  write(_sockfd, message.c_str(), message.length());
}

void NetClient::SendLocationUpdate(Entity *entity, worldbox::GetEntityLocation msg) {
  if (!entity) return;
  btTransform trans;
  entity->_rigidBody->getMotionState()->getWorldTransform(trans);

  worldbox::EntityLocation msg2;
  msg2.set_id(msg.id());
  if (msg.include_position()) {
    Vec3 pos;
    pos.Set(trans.getOrigin());
    worldbox::Vec3 *v = new worldbox::Vec3();
    pos.ToProtobuf(*v);
    msg2.set_allocated_position(v);
  }
  if (msg.include_rotation()) {
    Vec3 rot;
    btQuaternion q = trans.getRotation();
    btVector3 v = q.getAxis();
    v *= q.getAngle();
    rot.Set(v);
    worldbox::Vec3 *v2 = new worldbox::Vec3();
    rot.ToProtobuf(*v2);
    msg2.set_allocated_rotation(v2);
  }

  std::string str;
  msg2.SerializeToString(&str);
  SendMessage(0x0202, str);
}

void NetClient::MessageReceiver(void *userdata, std::string channel, v8::Handle<v8::Value> message) {
  NetClient *client = (NetClient*)userdata;
  if (!client->_is_open) return;

  // JSON stringify the message, then send it...
  std::string msg_str = jsonStringify(message);

  worldbox::MsgBroadcast msg;
  msg.set_json(msg_str);
  msg.set_channel(channel);
  std::string str;
  msg.SerializeToString(&str);

  client->SendMessage(0x0101, str);
}

NetClient::NetClient(int sockfd, struct sockaddr_in addr)
{
  _sockfd = sockfd;
  _addr = addr;
  _is_open = true;
}

void NetClient::update(double dt)
{
  if (!_is_open) return;

  // See if there are any messages waiting for us
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(_sockfd, &set);
  int retval = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
  if (retval == 1) {
    // Read the header
    unsigned short msgsize, msgtype;
    int nread = read(_sockfd, &msgsize, 2);
    if (nread == 0) {
      printf("Closing connection...\n");
      _is_open = false;
      close(_sockfd);
      return;
      while (1);
    }
    nread = read(_sockfd, &msgtype, 2);
    msgsize = ntohs(msgsize);
    msgtype = ntohs(msgtype);

    // Read the body
    char *buf = new char[msgsize];
    nread = read(_sockfd, buf, msgsize);

    // Parse the body
    if (msgtype == 0x0001) {
      worldbox::Hello msg;
      msg.ParseFromArray(buf, msgsize);
    } else if (msgtype == 0x0002) {
      worldbox::GlobalWorldSettings msg;
      msg.ParseFromArray(buf, msgsize);
      if (msg.has_physics_gravity()) {
	Vec3 grav;
	grav.Set(msg.physics_gravity());
	physics_world->setGravity(grav.ToBtVec());
      }
    } else if (msgtype == 0x0101) {
      // MsgBroadcast
      worldbox::MsgBroadcast msg;
      msg.ParseFromArray(buf, msgsize);
      HandleScope handle_scope(Isolate::GetCurrent());
      MsgBroadcast(msg.channel(), String::NewFromUtf8(Isolate::GetCurrent(),msg.json().c_str()));
    } else if (msgtype == 0x0102) {
      // MsgSubscribe
      worldbox::MsgSubscribe msg;
      msg.ParseFromArray(buf, msgsize);
      if (msg.unsubscribe()) {
	// How do we unsubscribe?
      } else {
	// Subscribe to the given channel...
	printf("Subscribing client to channel %s...\n", msg.channel().c_str());
	MessageSubscriber sub;
	sub.channel_name = msg.channel();
	sub._userdata = this;
	sub._cb = NetClient::MessageReceiver;
	MsgAddSubscriber(sub);
      }
    } else if (msgtype == 0x0201) {
      // SpawnEntity
      worldbox::SpawnEntity msg;
      msg.ParseFromArray(buf, msgsize);
      Vec3 pos;
      if (msg.has_start_position()) {
	pos.SetXYZ(msg.start_position().x(),
		   msg.start_position().y(),
		   msg.start_position().z());
      }
      int64_t id;
      if (msg.has_id()) {
	id = msg.id();
      } else {
	// Generate a random (TODO: unique) one...
	int64_t lo = rand();
	int64_t hi = rand();
	id = (hi<<32) | lo;
      }
      std::string config_str;
      if (msg.has_cfg_filename()) {
	config_str = readFile(msg.cfg_filename().c_str());
      }
      if (msg.has_cfg_json()) {
	config_str = msg.cfg_json();
      }
      std::string init_cfg_str;
      if (msg.has_config_string()) {
	init_cfg_str = msg.config_string();
      }
      SpawnEntity(config_str, pos, id, init_cfg_str, Isolate::GetCurrent());
    } else if (msgtype == 0x0202) {
      // GetEntityLocation
      worldbox::GetEntityLocation msg;
      msg.ParseFromArray(buf, msgsize);
      int64_t id = msg.id();
      if (msg.recurring()) {
	// Add them to a subscription catalog
	NetLocationSubscription *sub = new NetLocationSubscription();
	sub->_subscription = msg;
	sub->_countdown = 0.0;
	_location_subscriptions.push_back(sub);
      } else {
	// Send them the position right away
	Entity *entity = GetEntityById(id);
	SendLocationUpdate(entity, msg);
      }
    }

    delete buf;
  }

  // Update everyone's location subscriptions
  for (std::vector<NetLocationSubscription*>::iterator it=_location_subscriptions.begin(); it!=_location_subscriptions.end(); ++it) {
    NetLocationSubscription *sub = *it;
    sub->_countdown -= dt;
    if (sub->_countdown < 0.0) {
      sub->_countdown = sub->_subscription.period();
      SendLocationUpdate(GetEntityById(sub->_subscription.id()), sub->_subscription);
    }
  }
}

void NetServer::startServer()
{
  _server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (_server_sockfd < 0) {
    // Error...
    fprintf(stderr, "Unable to get a socket to start server on.\n");
  }
  struct sockaddr_in address;
  bzero((char*)&address, sizeof(address));
  int port = 15538;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  int optval = 1;
  setsockopt(_server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  if (bind(_server_sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    // Error...
    fprintf(stderr, "Unable to bind on server socket.\n");
  }
  listen(_server_sockfd, 5);
}

void NetServer::SendMessageToAll(int msgtype, std::string msg) {
  for (std::vector<NetClient*>::iterator it=_clients.begin(); it!=_clients.end(); ++it) {
    (*it)->SendMessage(msgtype, msg);
  }
}

void NetServer::update(double dt)
{
  // Select on the listening socket, see if we should accept
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(_server_sockfd, &set);
  int retval = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
  if (retval == 1) {
    // We can accept...
    socklen_t len;
    struct sockaddr_in addr; // Client's address
    int sockfd = accept(_server_sockfd, (struct sockaddr*)&addr, &len);
    if (sockfd < 0) {
      // Error...
      fprintf(stderr, "Unable to accept client connection.\n");
    }

    // Deal with the client...
    NetClient *client = new NetClient(sockfd, addr);
    _clients.push_back(client);
  }

  // Update all the clients
  for (std::vector<NetClient*>::iterator it=_clients.begin(); it!=_clients.end(); ++it) {
    (*it)->update(dt);
  }
}
