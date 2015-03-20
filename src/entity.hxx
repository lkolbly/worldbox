#ifndef ENTITY_HXX
#define ENTITY_HXX 1

#include<map>
#include<include/v8.h>
#include<bullet/btBulletDynamicsCommon.h>

#include "vec3.hxx"

class Entity {
public:
  static v8::Handle<v8::ObjectTemplate> MakeEntityTemplate(v8::Isolate *isolate);
  static void ApplyForce(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Die(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IdAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void PositionAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void SetCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Handle<v8::Object> Wrap(Entity *entity, v8::Isolate *isolate);
  static void MsgSubscribe(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MsgBroadcast2(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReceiveMessage(void *userdata, std::string channel, v8::Handle<v8::Value> message);
  static void SpawnEntity2(const v8::FunctionCallbackInfo<v8::Value>& args);

  int64_t _id;
  bool _toremove;

  std::map<std::string,v8::Persistent<v8::Function>> functions;

  // Physics properties
  btRigidBody *_rigidBody;
  btDefaultMotionState *_motionState;
  btCompoundShape *_shape;

  // v8 properties
  v8::Handle<v8::ObjectTemplate> global;
  v8::Persistent<v8::Context> context;
};

#endif
