#include<map>
#include<vector>
#include<cstdio>
#include<cerrno>
#include<sys/types.h> 
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<include/v8.h>
#include<include/v8-profiler.h>
#include<include/libplatform/libplatform.h>
#include<bullet/btBulletDynamicsCommon.h>
#include<rapidjson/document.h>

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

Vec3 ParseVec3Json(const rapidjson::Value& e) {
  Vec3 v;
  v.SetXYZ(e["x"].GetDouble(), e["y"].GetDouble(), e["z"].GetDouble());
  return v;
}

void ParseEntityShapeJson(btCompoundShape *root, const rapidjson::Value& doc) {
  for (int i=0; i<doc.Size(); i++) {
    const rapidjson::Value& elem = doc[i];
    if (elem.HasMember("primitive")) {
      // It's a primitive...
      btCollisionShape *shape = NULL;
      if (strcmp(elem["primitive"].GetString(),"box") == 0) {
	// It's a box
	Vec3 size = ParseVec3Json(elem["size"]);
	size = size*0.5; // To convert from full-extents to half-extents
	shape = new btBoxShape(size.ToBtVec());
      } else if (strcmp(elem["primitive"].GetString(),"cylinder") == 0) {
	// It's a cylinder
	double rx = elem["rx"].GetDouble();
	double rz = elem["rz"].GetDouble();
	double height = elem["height"].GetDouble();
	shape = new btCylinderShape(btVector3(rx,rz,height/2.0));
      } else if (strcmp(elem["primitive"].GetString(),"sphere") == 0) {
	// It's a sphere
	double r = elem["r"].GetDouble();
	shape = new btSphereShape(r);
      } else if (strcmp(elem["primitive"].GetString(),"capsule") == 0) {
	// It's a capsule (pill-shaped)
	// Height is the TOTAL height. r is the radius.
	double r = elem["r"].GetDouble();
	double h = elem["height"].GetDouble();
	shape = new btCapsuleShape(r, h-2.0*r);
      }

      // Position & rotation are common to all
      btTransform trans;
      trans.setIdentity();
      if (elem.HasMember("position")) {
	Vec3 pos = ParseVec3Json(elem["position"]);
	trans.setOrigin(pos.ToBtVec());
      }
      if (elem.HasMember("rotation")) {
	btQuaternion qt;
	Vec3 rot_axis = ParseVec3Json(elem["rotation"]["axis"]);
	double rot_amt = elem["rotation"]["amt"].GetDouble();
	qt.setRotation(rot_axis.ToBtVec(), rot_amt*M_PI/180.0);
	trans.setRotation(qt);
      }
      root->addChildShape(trans, shape);
    }
  }
}

// filename is the filename of the JS file that runs the entity
// Each entity JS script is run in a different context so they don't interfere
void SpawnEntity(std::string config_str, Vec3 position, Isolate *isolate) {
  // Parse the JSON file, figure out what we need to figure out
  rapidjson::Document config_doc;
  config_doc.Parse(config_str.c_str());

  const char *script_filename = config_doc["scriptFilename"].GetString();
  printf("Script filename: %s\n", script_filename);

  // Create the physics shape
  btCompoundShape *shape = new btCompoundShape();
  double mass = 0.0;
  bool hasmass = false;
  if (config_doc.HasMember("physics")) {
    if (config_doc["physics"].HasMember("mass")) {
      mass = config_doc["physics"]["mass"].GetDouble();
      hasmass = true;
    }
    if (config_doc["physics"].HasMember("shape")) {
      ParseEntityShapeJson(shape, config_doc["physics"]["shape"]);
    }
  } else {
    btTransform trans;
    trans.setIdentity();
    shape->addChildShape(trans, new btEmptyShape());
    mass = 0.0;
  }

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

  // Setup the physics object for it...
  btTransform transform;
  transform.setIdentity();
  transform.setOrigin(btVector3(position.GetX(),position.GetY(),position.GetZ()));
  btVector3 localInertia(0,0,0);
  if (hasmass)
    shape->calculateLocalInertia(mass,localInertia);
  btDefaultMotionState *motionstate = new btDefaultMotionState(transform);
  btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,motionstate,shape,localInertia);
  rbInfo.m_friction = 1.0;
  btRigidBody *body = new btRigidBody(rbInfo);
  printf("We have rigid body: %p\n",body);
  entity->_rigidBody = body;
  entity->_motionState = motionstate;
  entity->_shape = shape;
  physics_world->addRigidBody(body);

  // Add it to the ESC
  Handle<Object> v8entity = Entity::Wrap(entity, isolate);
  esc->v8entity.Reset(isolate,v8entity);

  // Run the stdlib
  {
    Local<String> source = String::NewFromUtf8(isolate, readFile("stdlib.js").c_str());
    Local<Script> script = Script::Compile(source);
    script->Run();
  }

  // Create a string containing the JavaScript source code.
  std::string source_str = readFile(script_filename);
  printf("We got a source of %i bytes.\n", source_str.length());
  Local<String> source = String::NewFromUtf8(isolate, source_str.c_str());

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

void DeleteEntity(EntitySpawnContext *esc) {
  // Delete all of bullet's things...
  physics_world->removeRigidBody(esc->entity->_rigidBody);
  delete esc->entity->_motionState;
  for (int i=0; i<esc->entity->_shape->getNumChildShapes(); i++) {
    btCollisionShape *shape = esc->entity->_shape->getChildShape(i);
    delete shape;
  }
  delete esc->entity->_shape;
  delete esc->entity->_rigidBody;

  // Remove V8's stuff...
  esc->entity->context.Reset();
  for (std::map<std::string,Persistent<Function>>::iterator it=esc->entity->functions.begin(); it!=esc->entity->functions.end(); ++it) {
    it->second.Reset();
  }

  // Remove our bookkeeping structures...
  delete esc->entity;
  delete esc;
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
  void SendMessage(int type, std::string message);

  // Internal messaging...
  static void MessageReceiver(void *userdata, std::string channel, Handle<Value> message);
};

std::string jsonStringify(Handle<Value> message) {
  // Create a new context for this (hrm, probably expensive...)
  HandleScope handle_scope(Isolate::GetCurrent());
  Local<Context> context = Context::New(Isolate::GetCurrent());
  Context::Scope context_scope(context);

  Local<String> source = String::NewFromUtf8(Isolate::GetCurrent(), "function __internal__json_stringify__tmp(obj) { return JSON.stringify(obj); }");
  Local<Script> script = Script::Compile(source);
  script->Run();

  Handle<Object> global = context->Global();
  Handle<Value> fn_val = global->Get(String::NewFromUtf8(Isolate::GetCurrent(), "__internal__json_stringify__tmp"));
  //Handle<Value> fn_val = global->Get(String::NewFromUtf8(Isolate::GetCurrent(), "JSON.stringify"));
  Handle<Function> fn = Handle<Function>::Cast(fn_val);
  Handle<Value> args[1];
  args[0] = message;
  Handle<Value> result = fn->Call(global, 1, args);

  String::Utf8Value str(result);
  return *str;
}

void NetClient::SendMessage(int type, std::string message) {
  unsigned short msgsize=htons(message.length()), msgtype=htons(type);
  write(_sockfd, &msgsize, 2);
  write(_sockfd, &msgtype, 2);
  write(_sockfd, message.c_str(), message.length());
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
  /*unsigned short msgsize=htons(str.length()), msgtype=htons(0x0101);
  printf("Sending message to %i, size=%X type=%X\n",client->_sockfd, msgsize, msgtype);
  write(client->_sockfd, &msgsize, 2);
  write(client->_sockfd, &msgtype, 2);
  write(client->_sockfd, str.c_str(), str.length());*/
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
    printf("Read %X bytes from network.\n",nread);

    // Parse the body
    if (msgtype == 0x0001) {
      worldbox::Hello msg;
      msg.ParseFromArray(buf, msgsize);
      printf("Got hello with version=%i\n", msg.version());
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
	MessageSubscriber sub;// = new MessageSubscriber();
	sub.channel_name = msg.channel();
	sub._userdata = this;
	sub._cb = NetClient::MessageReceiver;
	MsgAddSubscriber(sub);
      }
    } else if (msgtype == 0x0201) {
      // SpawnEntity
      worldbox::SpawnEntity msg;
      bool retval = msg.ParseFromArray(buf, msgsize);
      printf("Return val: %i\n",retval?1:0);
      for (int i=0; i<msgsize; i++) {
	printf("%02X",buf[i]);
      }
      printf("\n");
      Vec3 pos;
      if (msg.has_start_position()) {
	pos.SetXYZ(msg.start_position().x(),
		   msg.start_position().y(),
		   msg.start_position().z());
	printf("Position: %f,%f,%f -> %f,%f,%f\n", msg.start_position().x(), msg.start_position().y(), msg.start_position().z(), pos.GetX(), pos.GetY(), pos.GetZ());
      }
      std::string config_str;
      if (msg.has_cfg_filename()) {
	config_str = readFile(msg.cfg_filename().c_str());
      }
      if (msg.has_cfg_json()) {
	config_str = msg.cfg_json();
      }
      SpawnEntity(config_str, pos, Isolate::GetCurrent());
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

class myv8FileOutputStream : public OutputStream {
private:
  std::FILE *_f;
public:
  void EndOfStream();
  int GetChunkSize() { return 1; };
  int GetOutputEncoding() { return 0; };
  WriteResult WriteAsciiChunk(char *data, int size);
  myv8FileOutputStream(const char *filename);
  ~myv8FileOutputStream();
};

myv8FileOutputStream::myv8FileOutputStream(const char *filename) {
  _f = std::fopen(filename, "w");
  if (!_f) {
    fprintf(stderr, "Error opening %s for writing...\n", filename);
  }
}

myv8FileOutputStream::~myv8FileOutputStream() {
  // Do nothing...
}

void myv8FileOutputStream::EndOfStream() {
  std::fclose(_f);
}

OutputStream::WriteResult myv8FileOutputStream::WriteAsciiChunk(char *data, int size) {
  std::fwrite(data, size, 1, _f);
  return kContinue;
}

//HeapProfiler *heap_Profiler;

void DumpHeapProfile(const char *filename)
{
  HandleScope handle_scope(Isolate::GetCurrent());
  HeapProfiler *heap_Profiler = Isolate::GetCurrent()->GetHeapProfiler();
  const HeapSnapshot *snap = heap_Profiler->TakeHeapSnapshot(String::NewFromUtf8(Isolate::GetCurrent(),"snap"));
  //HeapSnapshot snap;
  myv8FileOutputStream out(filename);
  snap->Serialize(&out, HeapSnapshot::SerializationFormat::kJSON);
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
  physics_world->setGravity(btVector3(0,-10,0));

#if 0
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
#endif

  // Create a new Isolate and make it the current one.
  Isolate* isolate = Isolate::New();

  // Start the network server...
  NetServer net;
  net.startServer();

  {
    Isolate::Scope isolate_scope(isolate);

    //SpawnEntity("test.js", isolate);
    //SpawnEntity("test2.js", isolate);

    // Run the game loop
    double tgt_dt = 0.05;
    //time_t last_tm = time(NULL);
    struct timespec last_tm;
    clock_gettime(CLOCK_MONOTONIC, &last_tm);
    double dt = 0.01;
    int loopnum = 0;
    while (1) {
      for (std::vector<EntitySpawnContext*>::iterator it=esc_List.begin() ; it!=esc_List.end(); /*noop*/) {
        curesc = *it;

	// Skip if we don't have an update callback
	if (!curesc->entity->functions.count("update")) {
	  ++it;
	  continue;
	}

	HandleScope handle_scope(isolate);
	Handle<Value> fnargs[1];
	//fnargs[0] = esc->v8entity;//entity;
	fnargs[0] = Number::New(isolate, dt);
	Local<Function> fn = Local<Function>::New(isolate, curesc->entity->functions["update"]);
	Local<Context> ctx = Local<Context>::New(isolate, curesc->entity->context);

	TryCatch trycatch(isolate);
	printf("About to run function...\n");
	Local<Value> v = fn->Call(ctx->Global(), 1, fnargs);
	printf("%i\n",trycatch.HasCaught()?1:0);
	if (trycatch.HasCaught()) {
	  Local<Value> exception = trycatch.Exception();
	  String::Utf8Value exception_str(exception);
	  printf("Exception: %s\n", *exception_str);
	}

	if (curesc->entity->_toremove) {
	  // TODO: Clear all the persistent stuff...
	  printf("Removing...\n");
	  DeleteEntity(curesc);
	  it = esc_List.erase(it);
	} else {
	  ++it;
	}
      }

      // Update the physics...
      physics_world->stepSimulation(dt, 100, 1.0/100.0);

      // Update the network
      net.update();

      // Update the timestep
      struct timespec curtime;
      clock_gettime(CLOCK_MONOTONIC, &curtime);
      dt = curtime.tv_sec-last_tm.tv_sec + (double)(curtime.tv_nsec-last_tm.tv_nsec)/1000000000.0;
      while (tgt_dt > dt) {
	net.update(); // Alternatively, we could sleep
 	clock_gettime(CLOCK_MONOTONIC, &curtime);
	dt = curtime.tv_sec-last_tm.tv_sec + (double)(curtime.tv_nsec-last_tm.tv_nsec)/1000000000.0;
      }
      //printf("dt=%.3f\n",dt);
      last_tm = curtime;
      loopnum++;
      fflush(stdout);

      if (loopnum%100 == 0) {
	// Print out heap info
	HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);
	printf("V8 is using %i of %i bytes on the heap.\n", heap.used_heap_size(), heap.total_heap_size());
      }

      if (loopnum%1000 == 0) {
	DumpHeapProfile("profile.prof");
      }
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
