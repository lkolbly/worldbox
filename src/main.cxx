#include<map>
#include<vector>
#include<cstdio>
#include<cerrno>
#include<include/v8.h>
#include<include/libplatform/libplatform.h>
#include<bullet/btBulletDynamicsCommon.h>

#include "vec3.hxx"
#include "entity.hxx"

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

      if (esc_List.size() == 0) break;
      printf("Going around game loop again, %i ESC elements.\n", esc_List.size());
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
