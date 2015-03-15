#include<map>
#include<vector>
#include<cstdio>
#include<cerrno>
#include<sys/types.h> 
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<include/v8.h>
#include<include/libplatform/libplatform.h>
#include<bullet/btBulletDynamicsCommon.h>

#include "vec3.hxx"
#include "entity.hxx"
#include "messaging.hxx"
#include "net/messages.pb.h"

using namespace v8;

std::string readFile(const char *pathname);

v8::Handle<v8::ObjectTemplate> curglobal;
Local<Context> curcontext;

btDiscreteDynamicsWorld *physics_world;

class Entity;

class EntitySpawnContext {
public:
  //Handle<ObjectTemplate> global;
  //Persistent<Context> context;

  Entity *entity;
  Persistent<Object> v8entity;
};

EntitySpawnContext *curesc;
std::vector<EntitySpawnContext*> esc_List;

// From v8 examples (shell.cc)
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    v8::String::Utf8Value str(args[i]);
    const char* cstr = *str;//ToCString(str);
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}

void globalEntityGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  printf("Retrieving the current entity....\n");
  info.GetReturnValue().Set(curesc->v8entity);
}

// filename is the filename of the JS file that runs the entity
// Each entity JS script is run in a different context so they don't interfere
void SpawnEntity(const char *filename, Isolate *isolate) {
  // Create a stack-allocated handle scope.
  //Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  printf("Creating an esc...\n");
  EntitySpawnContext *esc = new EntitySpawnContext();
  Entity *entity = new Entity();
  esc->entity = entity;

  // Setup a global context (with print...)
  printf("Setting up a global entity...\n");
  entity->global = v8::ObjectTemplate::New(isolate);
  entity->global->Set(v8::String::NewFromUtf8(isolate, "print"),
		      v8::FunctionTemplate::New(isolate, Print));
  entity->global->SetAccessor(v8::String::NewFromUtf8(isolate, "entity"),
			      globalEntityGetter);

  // Create a new context.
  Handle<Context> context = Context::New(isolate, NULL, entity->global);
  entity->context.Reset(isolate,context);

  // Enter the context for compiling and running the hello world script.
  Context::Scope context_scope(context);

  // Setup the entity...

  // Setup the physics object for it...
  btBoxShape *shape = new btBoxShape(btVector3(1,1,1));
  btTransform transform;
  transform.setIdentity();
  transform.setOrigin(btVector3(0,3,0));
  btVector3 localInertia(0,0,0);
  double mass = 1.0;
  shape->calculateLocalInertia(mass,localInertia);
  btDefaultMotionState *motionstate = new btDefaultMotionState(transform);
  btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,motionstate,shape,localInertia);
  rbInfo.m_friction = 0.01;
  btRigidBody *body = new btRigidBody(rbInfo);
  printf("We have rigid body: %p\n",body);
  entity->_rigidBody = body;
  physics_world->addRigidBody(body);

  // Add it to the ESC
  //esc->entity = entity;
  Handle<Object> v8entity = Entity::Wrap(entity, isolate);
  esc->v8entity.Reset(isolate,v8entity);

  // Run the stdlib
  {
    Local<String> source = String::NewFromUtf8(isolate, readFile("stdlib.js").c_str());
    Local<Script> script = Script::Compile(source);
    Local<Value> result = script->Run();
  }

  // Create a string containing the JavaScript source code.
  Local<String> source = String::NewFromUtf8(isolate, readFile(filename).c_str());

  // Compile the source code.
  //curcontext = esc->context; // Set the current context
  curesc = esc;
  Local<Script> script = Script::Compile(source);

  // Run the script to get the result.
  Local<Value> result = script->Run();

  // Convert the result to an UTF8 string and print it.
  String::Utf8Value utf8(result);
  printf("Result: %s\n", *utf8);

  esc_List.push_back(esc);
}

std::string readFile(const char *pathname)
{
  std::FILE *f = std::fopen(pathname, "r");
  if (!f) {
    throw(errno);
  }
  std::string s;
  std::fseek(f, 0, SEEK_END);
  s.resize(std::ftell(f));
  std::rewind(f);
  std::fread(&s[0], 1, s.size(), f);
  std::fclose(f);
  return s;
}

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

public:
  NetClient(int sockfd, struct sockaddr_in addr);
  void update();

  // Internal messaging...
  static void MessageReceiver(void *userdata, std::string channel, Handle<Value> message);
};

void NetClient::MessageReceiver(void *userdata, std::string channel, v8::Handle<v8::Value> message) {
  NetClient *client = (NetClient*)userdata;
  if (!client->_is_open) return;

  // Convert message to a string, then send it...
  String::Utf8Value utf8(message);

  worldbox::MsgBroadcast msg;
  msg.set_msg(*utf8);
  std::string str;
  msg.SerializeToString(&str);

  unsigned short msgsize=htons(str.length()), msgtype=htons(0x0101);
  printf("Sending message to %i, size=%X type=%X\n",client->_sockfd, msgsize, msgtype);
  write(client->_sockfd, &msgsize, 2);
  write(client->_sockfd, &msgtype, 2);
  write(client->_sockfd, str.c_str(), str.length());
}

NetClient::NetClient(int sockfd, struct sockaddr_in addr)
{
  _sockfd = sockfd;
  _addr = addr;
  _is_open = true;

  // Send a quick hello...
  //write(_sockfd, "Hello!", 7);
}

void NetClient::update()
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
    printf("Got a message!\n");

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
    //msgsize = ((msgsize&0xFF)<<8) | ((msgsize&0xFF00)>>8);

    // Read the body
    printf("%X bytes of body (type=%X)...\n", msgsize, msgtype);
    char *buf = new char[msgsize];
    nread = read(_sockfd, buf, msgsize);

    // Parse the body
    if (msgtype == 0x0001) {
      worldbox::Hello msg;
      msg.ParseFromString(buf);
      printf("Got hello with version=%i\n", msg.version());
    } else if (msgtype == 0x0101) {
      // MsgBroadcast
      worldbox::MsgBroadcast msg;
      msg.ParseFromString(buf);
      HandleScope handle_scope(Isolate::GetCurrent());
      MsgBroadcast(msg.channel(), String::NewFromUtf8(Isolate::GetCurrent(),msg.msg().c_str()));
    } else if (msgtype == 0x0102) {
      // MsgSubscribe
      worldbox::MsgSubscribe msg;
      msg.ParseFromString(buf);
      if (msg.unsubscribe()) {
	// How do we unsubscribe?
      } else {
	// Subscribe to the given channel...
	printf("Subscribing client to channel %s...\n", msg.channel().c_str());
	MessageSubscriber sub;// = new MessageSubscriber();
	sub.channel_name = msg.channel();
	sub._userdata = this;
	sub._cb = NetClient::MessageReceiver;
	MsgAddSubscriber(sub);
      }
    }

    delete buf;

    /*char buf[256];
    int nread = read(_sockfd, buf, 256);
    buf[nread] = 0;
    printf("%s\n",buf);*/
  }
}

class NetServer {
private:
  int _server_sockfd;
  std::vector<NetClient*> _clients;

public:
  void startServer();
  void update();
};

void NetServer::startServer()
{
  _server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (_server_sockfd < 0) {
    // Error...
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
  }
  listen(_server_sockfd, 5);
}

void NetServer::update()
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
    }

    // Deal with the client...
    NetClient *client = new NetClient(sockfd, addr);
    _clients.push_back(client);
  }

  // Update all the clients
  for (std::vector<NetClient*>::iterator it=_clients.begin(); it!=_clients.end(); ++it) {
    (*it)->update();
  }
}

int main(int argc, char **argv)
{
  // Initialize V8.
  V8::InitializeICU();
  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();

  // Initialize Bullet
  btDefaultCollisionConfiguration *collisionConfig = new btDefaultCollisionConfiguration();
  btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfig);
  btBroadphaseInterface *pairCache = new btDbvtBroadphase();
  btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
  physics_world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, collisionConfig);
  physics_world->setGravity(btVector3(0,0,0));

  // Create the ground...
  {
    btBoxShape *shape = new btBoxShape(btVector3(50,50,50));
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(0,-50,0));
    btVector3 localInertia(0,0,0);
    double mass = 0.0; // Massless...
    //shape->calculateLocalInertia(mass,localInertia)
    btDefaultMotionState *motionstate = new btDefaultMotionState(transform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,motionstate,shape,localInertia);
    rbInfo.m_friction = 1.0;
    btRigidBody *body = new btRigidBody(rbInfo);
    physics_world->addRigidBody(body);
  }

  // Create a new Isolate and make it the current one.
  Isolate* isolate = Isolate::New();

  // Start the network server...
  NetServer net;
  net.startServer();

  {
    Isolate::Scope isolate_scope(isolate);

    SpawnEntity("test.js", isolate);
    SpawnEntity("test2.js", isolate);

    // Run the game loop
    while (1) {
      for (std::vector<EntitySpawnContext*>::iterator it=esc_List.begin() ; it!=esc_List.end(); /*noop*/) {
        curesc = *it;
	HandleScope handle_scope(isolate);
	Handle<Value> fnargs[1];
	//fnargs[0] = esc->v8entity;//entity;
	fnargs[0] = Number::New(isolate, 0.25);
	Local<Function> fn = Local<Function>::New(isolate, curesc->entity->functions["update"]);
	Local<Context> ctx = Local<Context>::New(isolate, curesc->entity->context);

	fn->Call(ctx->Global(), 1, fnargs);

	if (curesc->entity->_toremove) {
	  // TODO: Clear all the persistent stuff...
	  printf("Removing...\n");
	  it = esc_List.erase(it);
	} else {
	  ++it;
	}
      }

      // Update the physics...
      double timestep = 0.25;
      physics_world->stepSimulation(timestep, 100, 1.0/100.0);

      // Update the network
      net.update();

      //if (esc_List.size() == 0) break;
      //printf("Going around game loop again, %i ESC elements.\n", esc_List.size());
    }
  }

  printf("Exiting...\n");

  // Delete all the physics stuff...

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  delete platform;
  return 0;
}
