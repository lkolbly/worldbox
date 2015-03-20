#include<string>
#include<string.h>
#include<include/v8.h>
#include<bullet/btBulletDynamicsCommon.h>

#include "vec3.hxx"
#include "net/messages.pb.h"

using namespace v8;

void Vec3::Set(worldbox::Vec3 v)
{
  SetXYZ(v.x(), v.y(), v.z());
}

void Vec3::Set(btVector3 v)
{
  SetXYZ(double(v.getX()), double(v.getY()), double(v.getZ()));
}

void Vec3::SetXYZ(double x, double y, double z)
{
  _x = x;
  _y = y;
  _z = z;
}

void Vec3::ToProtobuf(worldbox::Vec3& v)
{
  v.set_x(_x);
  v.set_y(_y);
  v.set_z(_z);
}

btVector3 Vec3::ToBtVec()
{
  return btVector3(_x, _y, _z);
}

Vec3 Vec3::operator*(double scalar)
{
  Vec3 v;
  v.SetXYZ(_x*scalar, _y*scalar, _z*scalar);
  return v;
}

void Vec3::GetXYZ(Local<Name> name, const PropertyCallbackInfo<Value>& info)
{
  // Unwrap the vector
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Vec3 *v = (Vec3*)field->Value();

  std::string str(*String::Utf8Value(name));

  double val = 0.0;
  if (strcmp(str.c_str(), "x") == 0) {
    val = v->_x;
  } else if (strcmp(str.c_str(), "y") == 0) {
    val = v->_y;
  } else if (strcmp(str.c_str(), "z") == 0) {
    val = v->_z;
  }
  info.GetReturnValue().Set(Number::New(info.GetIsolate(), val));
}

void Vec3::SetXYZ(Local<Name> name, Local<Value> newval, const PropertyCallbackInfo<void>& info)
{
  // What does this even mean? We'd have to push the new location to physics.

  // Unwrap the vector
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Vec3 *v = (Vec3*)field->Value();

  std::string str(*String::Utf8Value(name));

  double val = newval->NumberValue();
  if (strcmp(str.c_str(), "x") == 0) {
    v->_x = val;
  } else if (strcmp(str.c_str(), "y") == 0) {
    v->_y = val;
  } else if (strcmp(str.c_str(), "z") == 0) {
    v->_z = val;
  }
}

void Vec3::ToString(const FunctionCallbackInfo<Value>& args) {
  // Unwrap the vector
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Vec3 *v = (Vec3*)field->Value();

  // Build a string...
  char *buf = new char[128];
  snprintf(buf, 128, "{x: %f, y: %f, z: %f}", v->_x, v->_y, v->_z);
  args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), buf));
}

void Vec3::ToStringAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(FunctionTemplate::New(info.GetIsolate(), Vec3::ToString)->GetFunction());
}

Persistent<ObjectTemplate> vec3_Entity_Template;

Handle<ObjectTemplate> Vec3::MakeTemplate(Isolate *isolate)
{
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate>jsentity = ObjectTemplate::New(isolate);
  jsentity->SetInternalFieldCount(1); // The only internal field is a Vec3
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "x", String::kInternalizedString), GetXYZ, SetXYZ);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "y", String::kInternalizedString), GetXYZ, SetXYZ);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "z", String::kInternalizedString), GetXYZ, SetXYZ);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "toString", String::kInternalizedString), ToStringAccessor);
  vec3_Entity_Template.Reset(isolate, jsentity);
  return handle_scope.Escape(jsentity);
}

Handle<Object> Vec3::Wrap(Vec3 *v, Isolate *isolate) {
  EscapableHandleScope handle_scope(isolate);
  if (vec3_Entity_Template.IsEmpty()) {
    Vec3::MakeTemplate(isolate);
  }
  //Handle<ObjectTemplate> entity_tpl = Vec3::MakeTemplate(isolate);
  Local<ObjectTemplate> entity_tpl = Local<ObjectTemplate>::New(isolate, vec3_Entity_Template);
  Local<Object> v8v = entity_tpl->NewInstance();
  Handle<External> v_ptr = External::New(isolate, v);
  v8v->SetInternalField(0, v_ptr);
  return handle_scope.Escape(v8v);
}
