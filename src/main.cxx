#include<string>
#include<string.h> // For strcmp, should probably change that
#include<map>
#include<vector>
#include<cstdio>
#include<cerrno>
#include<include/v8.h>
#include<include/libplatform/libplatform.h>

using namespace v8;

std::string readFile(const char *pathname);

v8::Handle<v8::ObjectTemplate> curglobal;
Local<Context> curcontext;

class Entity;

class Vec3 {
private:
  double _x, _y, _z;

public:
  static Handle<ObjectTemplate> MakeTemplate(Isolate *isolate);
  static Handle<Object> Wrap(Vec3 *v, Isolate *isolate);

  static void GetXYZ(Local<Name> name, const PropertyCallbackInfo<Value>& info);
  static void SetXYZ(Local<Name> name, Local<Value> newval, const PropertyCallbackInfo<void>& info);
  //static void GetXYZAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info);
  //static void SetXYZAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info);
};

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

class EntitySpawnContext {
public:
  Handle<ObjectTemplate> global;
  Persistent<Context> context;

  Entity *entity;
  Persistent<Object> v8entity;
};

EntitySpawnContext *curesc;
std::vector<EntitySpawnContext*> esc_List;

class Entity {
public:
  static Handle<ObjectTemplate> MakeEntityTemplate(Isolate *isolate);
  static void GetValue(Local<Name> name, const PropertyCallbackInfo<Value>& info);
  static void SetCallbackAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info);
  static void DieAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info);
  static void Die(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PositionAccessor(Local<Name> name, const PropertyCallbackInfo<Value>& info);

  int _value;
  bool _toremove;
  Vec3 _position;
  //Persistent<Function,CopyablePersistentTraits<Function>> updateFunction;
  Persistent<Function> updateFunction;

  //std::map<std::string,Persistent<Function,CopyablePersistentTraits<Function>>> functions;
  std::map<std::string,Persistent<Function>> functions;
};

Handle<Object> WrapEntity(Entity *entity, Isolate *isolate);

void Entity::Die(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();
  centity->_toremove = true;
}

void SetCallback2(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));
  Entity *centity = (Entity*)field->Value();
  centity->_value++;
  printf("this._value: %i\n", centity->_value);

  for (int i=0; i<args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    printf("Got an arg (i=%i)...\n", i);

    if (i==0) {
      // Should be a string...
      String::Utf8Value str(args[0]);
      printf("%s\n",*str);
    } else if (i==1) {
      //Entity *centity = new Entity();

      // Find the entity..
      //Handle<Object> entity = WrapEntity(centity, args.GetIsolate());
      //args.GetReturnValue().Set(WrapEntity(centity, args.GetIsolate()));

      /*String::Utf8Value str(args[1]);
      printf("%s\n",*str);*/
      // Persistent rather than Handle will prevent the GC from whacking fn
      Handle<Function> localhandle = Handle<Function>::Cast(args[1]);
      //Persistent<Function> fn = Persistent<Function>(args.GetIsolate(),localhandle);
      //Persistent<Function, CopyablePersistentTraits<Function>> persistent(args.GetIsolate(), localhandle);

      String::Utf8Value str2(args[0]);
      std::string str(*str2);

      // So we need to convert the local handle for the function to a
      // persistent handle. Then we need to add that to the functions map.
      // This is all done in this obtuse two lines of code.
      //std::pair<std::string, Persistent<Function, CopyablePersistentTraits<Function>>> fn_item(std::string(*str), Persistent<Function, CopyablePersistentTraits<Function>>(args.GetIsolate(), localhandle));
      //centity->functions.insert(fn_item);

      /*if (centity->functions.count(str) == 0) {
	// we need to create it...
	centity->functions.insert(std::pair<std::string, Persistent<Function>>(str, Persistent<Function>(args.GetIsolate(),localhandle)));
      } else {
	// we need to update it...
	centity->functions[str].Reset(args.GetIsolate(), localhandle);
      }*/

      centity->functions[str].Reset(args.GetIsolate(), localhandle);

      /*Persistent<Function> fn;
      fn.Reset(args.GetIsolate(), localhandle);
      centity->updateFunction.Reset(args.GetIsolate(), localhandle);*/


      /*std::pair<std::string, Persistent<Function>> fn_item(std::string(*str), fn);
	centity->functions.insert(fn_item);*/

      //centity->updateFunction = Persistent<Function, CopyablePersistentTraits<Function>>(args.GetIsolate(), localhandle);
      //centity->updateFunction = persistent;//localhandle;
      //Persistent<Function> fn = Persistent<Function>::New(args.GetIsolate(),*localhandle);
      //centity->updateFunction = fn;
      //handle_scope.Escape(centity->updateFunction);
      /*Handle<Object> recv;
      Handle<Value> fnargs[2];
      fnargs[0] = args.Holder();//entity;
      fnargs[1] = Integer::New(args.GetIsolate(), 123);
      fn->Call(curcontext->Global(), 2, fnargs);*/
    }
  }
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

  info.GetReturnValue().Set(FunctionTemplate::New(info.GetIsolate(), SetCallback2)->GetFunction());
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

Handle<Object> WrapEntity(Entity *entity, Isolate *isolate) {
  EscapableHandleScope handle_scope(isolate);
  Handle<ObjectTemplate> entity_tpl = Entity::MakeEntityTemplate(isolate);
  Local<Object> v8entity = entity_tpl->NewInstance();
  Handle<External> entity_ptr = External::New(isolate, entity);
  v8entity->SetInternalField(0, entity_ptr);
  return handle_scope.Escape(v8entity);
}

#if 0
void SetCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i=0; i<args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    printf("Got an arg (i=%i)...\n", i);

    if (i==0) {
      // Should be a string...
      String::Utf8Value str(args[0]);
      printf("%s\n",*str);
    } else if (i==1) {
      Entity *centity = new Entity();

      // Find the entity..
      Handle<Object> entity = WrapEntity(centity, args.GetIsolate());
      //args.GetReturnValue().Set(WrapEntity(centity, args.GetIsolate()));

      String::Utf8Value str(args[1]);
      printf("%s\n",*str);
      Handle<Function> fn = Handle<v8::Function>::Cast(args[1]);
      Handle<Object> recv;
      Handle<Value> fnargs[2];
      fnargs[0] = entity;
      fnargs[1] = Integer::New(args.GetIsolate(), 123);
      fn->Call(curcontext->Global(), 2, fnargs);
    }
  }
}
#endif

// Creates an entity class which is linked to JS, and returns it.
// Called from JS.
void MakeEntity(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Entity *entity = new Entity();
  curesc->entity = entity;
  Handle<Object> v8entity = WrapEntity(entity, args.GetIsolate());
  args.GetReturnValue().Set(v8entity);
}

void globalEntityGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  printf("Retrieving the current entity....\n");
  info.GetReturnValue().Set(curesc->v8entity);
}

void globalEntitySetter(Local<String> property, Local<Value> newval, const PropertyCallbackInfo<void>& info)
{
  // No-op. They can't set the property.
}

// filename is the filename of the JS file that runs the entity
// Each entity JS script is run in a different context so they don't interfere
void SpawnEntity(const char *filename, Isolate *isolate) {
  // Create a stack-allocated handle scope.
  //Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  printf("Creating an esc...\n");
  EntitySpawnContext *esc = new EntitySpawnContext();

  // Setup a global context (with print...)
  printf("Setting up a global entity...\n");
  esc->global = v8::ObjectTemplate::New(isolate);
  esc->global->Set(v8::String::NewFromUtf8(isolate, "print"),
		   v8::FunctionTemplate::New(isolate, Print));
  /*esc->global->Set(v8::String::NewFromUtf8(isolate, "setCallback"),
		   v8::FunctionTemplate::New(isolate, SetCallback));*/
  esc->global->Set(v8::String::NewFromUtf8(isolate, "makeEntity"),
		   v8::FunctionTemplate::New(isolate, MakeEntity));
  esc->global->SetAccessor(v8::String::NewFromUtf8(isolate, "entity"),
			   globalEntityGetter, globalEntitySetter);

  // Create a new context.
  Handle<Context> context = Context::New(isolate, NULL, esc->global);
  esc->context.Reset(isolate,context);

  // Enter the context for compiling and running the hello world script.
  Context::Scope context_scope(context);

  // Setup the entity...
  Entity *entity = new Entity();
  esc->entity = entity;
  Handle<Object> v8entity = WrapEntity(entity, isolate);
  esc->v8entity.Reset(isolate,v8entity);

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

  // Run the update function...
  //Handle<Object> recv;
  /*printf("Running functions...\n");
  Handle<Value> fnargs[2];
  //fnargs[0] = esc->v8entity;//entity;
  fnargs[0] = Integer::New(isolate, 123);
  Local<Function> fn = Local<Function>::New(isolate, esc->entity->functions["update"]);
  //Local<Function> fn = Local<Function>::New(isolate, esc->entity->updateFunction);
  fn->Call(context->Global(), 1, fnargs);*/
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

  // Create a new Isolate and make it the current one.
  Isolate* isolate = Isolate::New();

  {
    Isolate::Scope isolate_scope(isolate);
    //HandleScope handle_scope(isolate);

    SpawnEntity("test.js", isolate);
    SpawnEntity("test2.js", isolate);

    // Run the game loop
    while (1) {
      for (std::vector<EntitySpawnContext*>::iterator it=esc_List.begin() ; it!=esc_List.end(); /*noop*/) {
        curesc = *it;
	HandleScope handle_scope(isolate);
	Handle<Value> fnargs[1];
	//fnargs[0] = esc->v8entity;//entity;
	fnargs[0] = Integer::New(isolate, 123);
	Local<Function> fn = Local<Function>::New(isolate, curesc->entity->functions["update"]);
	//Local<Function> fn = Local<Function>::New(isolate, curesc->entity->updateFunction);
	Local<Context> ctx = Local<Context>::New(isolate, curesc->context);

	fn->Call(ctx->Global(), 1, fnargs);

	if (curesc->entity->_toremove) {
	  // TODO: Clear all the persistent stuff...
	  printf("Removing...\n");
	  it = esc_List.erase(it);
	} else {
	  ++it;
	}
      }

      if (esc_List.size() == 0) break;
      printf("Going around game loop again, %i ESC elements.\n", esc_List.size());
    }

#if 0
    // Create a new handlescope
    {
      HandleScope handle_scope(isolate);
      Handle<Value> fnargs[1];
      //fnargs[0] = esc->v8entity;//entity;
      fnargs[0] = Integer::New(isolate, 123);
      Local<Function> fn = Local<Function>::New(isolate, curesc->entity->functions["update"]);
      //Local<Function> fn = Local<Function>::New(isolate, curesc->entity->updateFunction);
      Local<Context> ctx = Local<Context>::New(isolate, curesc->context);

      fn->Call(ctx->Global(), 1, fnargs);
    }
#endif
  }

#if 0
  {
 // Destroy isolate_scope when we're done here
    Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate);

    // Setup a global context (with print...)
    global = v8::ObjectTemplate::New(isolate);
    global->Set(v8::String::NewFromUtf8(isolate, "print"),
		v8::FunctionTemplate::New(isolate, Print));
    global->Set(v8::String::NewFromUtf8(isolate, "setCallback"),
		v8::FunctionTemplate::New(isolate, SetCallback));
    global->Set(v8::String::NewFromUtf8(isolate, "makeEntity"),
		v8::FunctionTemplate::New(isolate, MakeEntity));

    // Create a new context.
    context = Context::New(isolate, NULL, global);

    // Enter the context for compiling and running the hello world script.
    Context::Scope context_scope(context);

    // Setup the template for entities...
    //Handle<ObjectTemplate> entity_tpl = Entity::MakeEntityTemplate(isolate);

    // Create a string containing the JavaScript source code.
    //Local<String> source = String::NewFromUtf8(isolate, "print('Hello' + ', World! '+(2+1));");
    Local<String> source = String::NewFromUtf8(isolate, readFile("test.js").c_str());

    // Compile the source code.
    Local<Script> script = Script::Compile(source);

    // Run the script to get the result.
    Local<Value> result = script->Run();

    // Convert the result to an UTF8 string and print it.
    String::Utf8Value utf8(result);
    printf("Result: %s\n", *utf8);
  }
#endif
  printf("Exiting...\n");

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  delete platform;
  return 0;
}
