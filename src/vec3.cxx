#include<string>
#include<string.h>
#include<include/v8.h>

#include "vec3.hxx"

using namespace v8;

void Vec3::GetXYZ(Local<Name> name, const PropertyCallbackInfo<Value>& info)
{
  // Unwrap the vector
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Vec3 *v = (Vec3*)field->Value();

  std::string str(*String::Utf8Value(name));
  printf("%s\n", str.c_str());

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

Handle<ObjectTemplate> Vec3::MakeTemplate(Isolate *isolate)
{
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate>jsentity = ObjectTemplate::New(isolate);
  jsentity->SetInternalFieldCount(1); // The only internal field is a Vec3
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "x", String::kInternalizedString), GetXYZ, SetXYZ);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "y", String::kInternalizedString), GetXYZ, SetXYZ);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "z", String::kInternalizedString), GetXYZ, SetXYZ);
  return handle_scope.Escape(jsentity);
}

Handle<Object> Vec3::Wrap(Vec3 *v, Isolate *isolate) {
  EscapableHandleScope handle_scope(isolate);
  Handle<ObjectTemplate> entity_tpl = Vec3::MakeTemplate(isolate);
  Local<Object> v8v = entity_tpl->NewInstance();
  Handle<External> v_ptr = External::New(isolate, v);
  v8v->SetInternalField(0, v_ptr);
  return handle_scope.Escape(v8v);
}
