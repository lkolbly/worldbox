#include<string>
#include<include/v8.h>
#include<bullet/btBulletDynamicsCommon.h>

#include "entity.hxx"
#include "messaging.hxx"
#include "util.hxx"

using namespace v8;

void SpawnEntity(std::string config_str, Vec3 position, int64_t id, std::string init_cfg_str, Isolate *isolate);

// Note: We assume we're all under the same isolate
class EntityMessageSubscriber {
public:
  Entity *entity;
  Persistent<Function> callback;
};

void Entity::ReceiveMessage(void *userdata, std::string channel, Handle<Value> message)
{
  // Pass along the message
  // Note: The callback cannot modify the message.
  EntityMessageSubscriber *sub = (EntityMessageSubscriber*)userdata;

  HandleScope handle_scope(Isolate::GetCurrent());
  Handle<Value> fnargs[1];
  fnargs[0] = message;
  Local<Function> fn = Local<Function>::New(Isolate::GetCurrent(), sub->callback);
  Local<Context> ctx = Local<Context>::New(Isolate::GetCurrent(), sub->entity->context);
  Context::Scope context_scope(ctx);

  fn->Call(ctx->Global(), 1, fnargs);
}

Handle<Object> Entity::Wrap(Entity *entity, Isolate *isolate) {
  EscapableHandleScope handle_scope(isolate);
  Handle<ObjectTemplate> entity_tpl = Entity::MakeEntityTemplate(isolate);
  Local<Object> v8entity = entity_tpl->NewInstance();
  Handle<External> entity_ptr = External::New(isolate, entity);
  v8entity->SetInternalField(0, entity_ptr);
  return handle_scope.Escape(v8entity);
}

void Entity::Die(const FunctionCallbackInfo<Value>& args) {
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();
  centity->_toremove = true;
}

void Entity::ApplyForce(const FunctionCallbackInfo<Value>& args) {
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  // Get the force's x, y, and z (and relpos)
  double x = args[0]->NumberValue();
  double y = args[1]->NumberValue();
  double z = args[2]->NumberValue();

  double rx = args[3]->NumberValue();
  double ry = args[4]->NumberValue();
  double rz = args[5]->NumberValue();

  // Convert force from local to world coordinates
  btTransform trans;
  btVector3 force(x,y,z);
  centity->_rigidBody->getMotionState()->getWorldTransform(trans);
  force = btTransform(trans.getRotation()) * force;

  // Convert relpos from local to world coords
  btVector3 relpos(rx,ry,rz);
  relpos = btTransform(trans.getRotation()) * relpos;

  // Apply the force
  centity->_rigidBody->applyForce(force, relpos);
  centity->_rigidBody->activate();

  force = centity->_rigidBody->getTotalForce();
}

void Entity::SetCallback(const FunctionCallbackInfo<Value>& args) {
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  if (args.Length() < 2) {
    // Throw an error!
    fprintf(stderr, "ERROR: args.Length < 2. I'm about to segfault.\n");
  }

  // Persistent rather than Handle will prevent the GC from whacking fn
  Handle<Function> localhandle = Handle<Function>::Cast(args[1]);

  String::Utf8Value str2(args[0]);
  std::string str(*str2);

  centity->functions[str].Reset(args.GetIsolate(), localhandle);
}

void Entity::MsgSubscribe(const FunctionCallbackInfo<Value>& args) {
  // Unwrap the entity
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  EntityMessageSubscriber *e_sub = new EntityMessageSubscriber();
  e_sub->entity = centity;

  Handle<Function> localhandle = Handle<Function>::Cast(args[1]);
  e_sub->callback.Reset(args.GetIsolate(), localhandle);

  // Subscribe the given function to the given channel
  MessageSubscriber subscriber;
  subscriber.channel_name = *String::Utf8Value(args[0]);
  subscriber._userdata = e_sub;
  subscriber._cb = Entity::ReceiveMessage;

  MsgAddSubscriber(subscriber);
}

void Entity::SpawnEntity2(const FunctionCallbackInfo<Value>& args) {
  std::string config_json_filename = *String::Utf8Value(args[0]);
  std::string init_cfg_str = *String::Utf8Value(args[1]);

  double x = args[2]->NumberValue();
  double y = args[3]->NumberValue();
  double z = args[4]->NumberValue();
  double rx = args[5]->NumberValue();
  double ry = args[6]->NumberValue();
  double rz = args[7]->NumberValue();

  Vec3 v;
  v.SetXYZ(x,y,z);

  int64_t lo = rand();
  int64_t hi = rand();
  int64_t id = (hi<<32) | lo;

  SpawnEntity(readFile(config_json_filename.c_str()), v, id, init_cfg_str, Isolate::GetCurrent());

  char *s = new char[32];
  snprintf(s,32,"%li", id);
  args.GetReturnValue().Set(String::NewFromUtf8(Isolate::GetCurrent(), s));
  delete s;
}

void Entity::MsgBroadcast2(const FunctionCallbackInfo<Value>& args) {
  std::string channel_name = *String::Utf8Value(args[0]);

  MsgBroadcast(channel_name, args[1]);
}

void Entity::IdAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  // Unwrap the entity
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  char *s = new char[32];
  snprintf(s, 32, "%li", centity->_id);
  info.GetReturnValue().Set(String::NewFromUtf8(Isolate::GetCurrent(), s));
  delete s;
}

void Entity::PositionAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  // Unwrap the entity
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  std::string str(*String::Utf8Value(name));
  Vec3 *pos = new Vec3();
  if (strcmp(str.c_str(), "position") == 0) {
    btTransform trans;
    centity->_rigidBody->getMotionState()->getWorldTransform(trans);
    pos->SetXYZ(double(trans.getOrigin().getX()), double(trans.getOrigin().getY()), double(trans.getOrigin().getZ()));
  } else if (strcmp(str.c_str(), "velocity") == 0) {
    btVector3 linvel = centity->_rigidBody->getLinearVelocity();
    btTransform trans;
    centity->_rigidBody->getMotionState()->getWorldTransform(trans);
    linvel = btTransform(trans.inverse().getRotation()) * linvel;
    pos->Set(linvel);
  } else if (strcmp(str.c_str(), "rotation") == 0) {
    // Returns it in rotation axis form
    btTransform trans;
    centity->_rigidBody->getMotionState()->getWorldTransform(trans);
    btQuaternion q = trans.getRotation();
    btVector3 v = q.getAxis();
    v *= q.getAngle();
    pos->Set(v);
  } else if (strcmp(str.c_str(), "rotvel") == 0) {
    btVector3 v = centity->_rigidBody->getAngularVelocity();
    btTransform trans;
    centity->_rigidBody->getMotionState()->getWorldTransform(trans);
    v = btTransform(trans.getRotation()) * v;
    pos->Set(v);
  }

  info.GetReturnValue().Set(Vec3::Wrap(pos, info.GetIsolate()));
}

Persistent<ObjectTemplate> entity_Template;

// Makes an entity, given these arguments
Handle<ObjectTemplate> Entity::MakeEntityTemplate(Isolate *isolate) {
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate> jsentity;

  if (entity_Template.IsEmpty()) {
    jsentity = ObjectTemplate::New(isolate);
    jsentity->SetInternalFieldCount(1);

    // Set all of the accessors
    jsentity->SetAccessor(String::NewFromUtf8(isolate, "id", String::kInternalizedString), IdAccessor);
    jsentity->SetAccessor(String::NewFromUtf8(isolate, "position", String::kInternalizedString), PositionAccessor);
    jsentity->SetAccessor(String::NewFromUtf8(isolate, "velocity", String::kInternalizedString), PositionAccessor);
    jsentity->SetAccessor(String::NewFromUtf8(isolate, "rotation", String::kInternalizedString), PositionAccessor);
    jsentity->SetAccessor(String::NewFromUtf8(isolate, "rotvel", String::kInternalizedString), PositionAccessor);

    // Set all of the functions
    jsentity->Set(String::NewFromUtf8(isolate, "setCallback", String::kInternalizedString), FunctionTemplate::New(isolate, Entity::SetCallback));
    jsentity->Set(String::NewFromUtf8(isolate, "markForRemoval", String::kInternalizedString), FunctionTemplate::New(isolate, Entity::Die));
    jsentity->Set(String::NewFromUtf8(isolate, "applyForce", String::kInternalizedString), FunctionTemplate::New(isolate, Entity::ApplyForce));
    jsentity->Set(String::NewFromUtf8(isolate, "subscribe", String::kInternalizedString), FunctionTemplate::New(isolate, Entity::MsgSubscribe));
    jsentity->Set(String::NewFromUtf8(isolate, "broadcast", String::kInternalizedString), FunctionTemplate::New(isolate, Entity::MsgBroadcast2));
    jsentity->Set(String::NewFromUtf8(isolate, "spawnEntity", String::kInternalizedString), FunctionTemplate::New(isolate, Entity::SpawnEntity2));

    entity_Template.Reset(isolate, jsentity);
  } else {
    jsentity = Local<ObjectTemplate>::New(isolate, entity_Template);
  }

  return handle_scope.Escape(jsentity);
}
