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
  static void ApplyForceAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>&info);
  static void GetValue(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void SetCallbackAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void DieAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void Die(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PositionAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void SetCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Handle<v8::Object> Wrap(Entity *entity, v8::Isolate *isolate);
  static void MsgSubscribe(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MsgSubscribeAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void MsgBroadcast2(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MsgBroadcastAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ReceiveMessage(void *userdata, std::string channel, v8::Handle<v8::Value> message);

  int _value;
  bool _toremove;
  Vec3 _position;

  std::map<std::string,v8::Persistent<v8::Function>> functions;

  // Physics properties
  btRigidBody *_rigidBody;
  btDefaultMotionState *_motionState;
  btCompoundShape *_shape;

  // v8 properties
  v8::Handle<v8::ObjectTemplate> global;
  v8::Persistent<v8::Context> context;
};

void BroadcastMessage(v8::Isolate *isolate, std::string channel, v8::Handle<v8::Value> message);

#endif
