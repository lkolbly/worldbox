#include<map>
#include<vector>
#include<cstdio>
#include<cerrno>
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
#include "net.hxx"
#include "util.hxx"

using namespace v8;

btDiscreteDynamicsWorld *physics_world;

NetServer net;

class EntitySpawnContext {
public:
  Entity *entity;
  Persistent<Object> v8entity;
};

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
    const char* cstr = *str;
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}

/*void globalEntityGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(curesc->v8entity);
}*/

Vec3 ParseVec3Json(const rapidjson::Value& e) {
  Vec3 v;
  v.SetXYZ(e["x"].GetDouble(), e["y"].GetDouble(), e["z"].GetDouble());
  return v;
}

void ParseEntityShapeJson(btCompoundShape *root, const rapidjson::Value& doc) {
  for (unsigned int i=0; i<doc.Size(); i++) {
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

// Tell all the network clients that there's a new entity...
void NotifyOfSpawnedEntity(std::string config_str, int64_t id, Vec3 pos) {
  worldbox::EntitySpawned msg;
  msg.set_id(id);
  msg.set_entity_type(config_str);
  worldbox::Vec3 *v = new worldbox::Vec3();
  pos.ToProtobuf(*v);
  msg.set_allocated_position(v);

  std::string str;
  msg.SerializeToString(&str);
  net.SendMessageToAll(0x0201, str);
}

// filename is the filename of the JS file that runs the entity
// Each entity JS script is run in a different context so they don't interfere
void SpawnEntity(std::string config_str, Vec3 position, int64_t id, std::string init_cfg_str, Isolate *isolate) {
  // Parse the JSON file, figure out what we need to figure out
  rapidjson::Document config_doc;
  config_doc.Parse(config_str.c_str());

  const char *script_filename = config_doc["scriptFilename"].GetString();
  printf("Spawning entity w/ script filename: %s\n", script_filename);

  const char *entity_type = config_doc["entityType"].GetString();

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
  HandleScope handle_scope(isolate);

  printf("Creating an esc...\n");
  EntitySpawnContext *esc = new EntitySpawnContext();
  Entity *entity = new Entity();
  entity->_id = id;
  esc->entity = entity;

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
  entity->global = ObjectTemplate::New(isolate);
  Handle<Context> context = Context::New(isolate, NULL, entity->global);

  // Enter the context for compiling and running the hello world script.
  Context::Scope context_scope(context);

  Handle<Object> v8entity = Entity::Wrap(entity, isolate);
  esc->v8entity.Reset(isolate,v8entity);

  // Setup a global context (with print...)
  printf("Setting up a global entity...\n");
  context->Global()->Set(v8::String::NewFromUtf8(isolate, "print"),
			 v8::FunctionTemplate::New(isolate, Print)->GetFunction());
  context->Global()->Set(String::NewFromUtf8(isolate, "entity"),
			 v8entity);
  context->Global()->Set(String::NewFromUtf8(isolate, "config"),
			 String::NewFromUtf8(isolate, init_cfg_str.c_str()));

  // Create a new context.
  entity->context.Reset(isolate,context);

  // Run the stdlib
  {
    Local<String> source = String::NewFromUtf8(isolate, readFile("stdlib.js").c_str());
    Local<Script> script = Script::Compile(source);
    script->Run();
  }

  // Create a string containing the JavaScript source code.
  std::string source_str = readFile(script_filename);
  printf("We got a source of %lu bytes.\n", source_str.length());
  Local<String> source = String::NewFromUtf8(isolate, source_str.c_str());

  // Compile the source code.
  Local<Script> script = Script::Compile(source);

  // Run the script to get the result.
  script->Run();

  esc_List.push_back(esc);

  NotifyOfSpawnedEntity(std::string(entity_type), id, position);
}

Entity *GetEntityById(int64_t id) {
  for (unsigned int i=0; i<esc_List.size(); i++) {
    EntitySpawnContext *esc = esc_List[i];
    if (esc->entity->_id == id) return esc->entity;
  }
  return NULL;
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

class myv8FileOutputStream : public OutputStream {
private:
  std::FILE *_f;
public:
  void EndOfStream();
  int GetChunkSize() { return 1; };
  int GetOutputEncoding() { return 0; };
  WriteResult WriteAsciiChunk(char *data, int size);
  myv8FileOutputStream(const char *filename);
  ~myv8FileOutputStream() { /* No-op */ };
};

myv8FileOutputStream::myv8FileOutputStream(const char *filename) {
  _f = std::fopen(filename, "w");
  if (!_f) {
    fprintf(stderr, "Error opening %s for writing...\n", filename);
  }
}

void myv8FileOutputStream::EndOfStream() {
  std::fclose(_f);
}

OutputStream::WriteResult myv8FileOutputStream::WriteAsciiChunk(char *data, int size) {
  std::fwrite(data, size, 1, _f);
  return kContinue;
}

void DumpHeapProfile(const char *filename)
{
  HandleScope handle_scope(Isolate::GetCurrent());
  HeapProfiler *heap_Profiler = Isolate::GetCurrent()->GetHeapProfiler();
  const HeapSnapshot *snap = heap_Profiler->TakeHeapSnapshot(String::NewFromUtf8(Isolate::GetCurrent(),"snap"));
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
  physics_world->setGravity(btVector3(0,0,0));

  // Create a new Isolate and make it the current one.
  Isolate* isolate = Isolate::New();

  // Start the network server...
  //NetServer net;
  net.startServer();

  {
    Isolate::Scope isolate_scope(isolate);

    // Run the game loop
    double tgt_dt = 0.05;
    struct timespec last_tm;
    clock_gettime(CLOCK_MONOTONIC, &last_tm);
    double dt = 0.01;
    int loopnum = 0;
    while (1) {
      for (std::vector<EntitySpawnContext*>::iterator it=esc_List.begin() ; it!=esc_List.end(); /*noop*/) {
        EntitySpawnContext *curesc = *it;

	// Skip if we don't have an update callback
	if (!curesc->entity->functions.count("update")) {
	  ++it;
	  continue;
	}

	HandleScope handle_scope(isolate);
	Handle<Value> fnargs[1];
	fnargs[0] = Number::New(isolate, dt);
	Local<Function> fn = Local<Function>::New(isolate, curesc->entity->functions["update"]);
	Local<Context> ctx = Local<Context>::New(isolate, curesc->entity->context);

	TryCatch trycatch(isolate);
	fn->Call(ctx->Global(), 1, fnargs);
	if (trycatch.HasCaught()) {
	  Local<Value> exception = trycatch.Exception();
	  String::Utf8Value exception_str(exception);
	  printf("Exception: %s\n", *exception_str);
	}

	if (curesc->entity->_toremove) {
	  // TODO: Clear all the persistent stuff...
	  printf("Removing entity...\n");
	  DeleteEntity(curesc);
	  it = esc_List.erase(it);
	} else {
	  ++it;
	}
      }

      // Update the physics...
      physics_world->stepSimulation(dt, 100, 1.0/100.0);

      // Update the network
      net.update(dt);

      // Update the timestep
      struct timespec curtime;
      clock_gettime(CLOCK_MONOTONIC, &curtime);
      dt = curtime.tv_sec-last_tm.tv_sec + (double)(curtime.tv_nsec-last_tm.tv_nsec)/1000000000.0;
      while (tgt_dt > dt) {
	//net.update(); // Alternatively, we could sleep
 	clock_gettime(CLOCK_MONOTONIC, &curtime);
	dt = curtime.tv_sec-last_tm.tv_sec + (double)(curtime.tv_nsec-last_tm.tv_nsec)/1000000000.0;
      }
      last_tm = curtime;
      loopnum++;
      fflush(stdout);

      if (loopnum%100 == 0) {
	// Print out heap info
	HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);
	printf("V8 is using %lu of %lu bytes on the heap.\n", heap.used_heap_size(), heap.total_heap_size());
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
