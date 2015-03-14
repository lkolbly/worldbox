#include<include/v8.h>

#include "entity.hxx"

using namespace v8;

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

void Entity::SetCallback(const FunctionCallbackInfo<Value>& args) {
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();
  centity->_value++;
  printf("this._value: %i\n", centity->_value);

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

void Entity::GetValue(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  // Unwrap the entity
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  //centity->_value++;
  info.GetReturnValue().Set(Integer::New(info.GetIsolate(), centity->_value));
}

void Entity::SetCallbackAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  // Unwrap the entity
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  info.GetReturnValue().Set(FunctionTemplate::New(info.GetIsolate(), Entity::SetCallback)->GetFunction());
}

void Entity::DieAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(FunctionTemplate::New(info.GetIsolate(), Entity::Die)->GetFunction());
}

void Entity::PositionAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  // Unwrap the entity
  Handle<External> field = Handle<External>::Cast(info.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();

  info.GetReturnValue().Set(Vec3::Wrap(&centity->_position, info.GetIsolate()));
}

// Makes an entity, given these arguments
Handle<ObjectTemplate> Entity::MakeEntityTemplate(Isolate *isolate) {
  EscapableHandleScope handle_scope(isolate);

  Local<ObjectTemplate> jsentity = ObjectTemplate::New(isolate);
  jsentity->SetInternalFieldCount(1);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "val", String::kInternalizedString), GetValue);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "setCallback", String::kInternalizedString), SetCallbackAccessor);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "markForRemoval", String::kInternalizedString), DieAccessor);
  jsentity->SetAccessor(String::NewFromUtf8(isolate, "position", String::kInternalizedString), PositionAccessor);

  return handle_scope.Escape(jsentity);
}
