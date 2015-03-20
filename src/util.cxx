#include<string>
#include<include/v8.h>

#include "util.hxx"

using namespace v8;

class JsonStringifyer {
private:
  Persistent<Context> _context;
public:
  JsonStringifyer();
  ~JsonStringifyer();
  std::string jsonStringify(Handle<Value> message);
};

JsonStringifyer::JsonStringifyer() {
  Local<Context> context = Context::New(Isolate::GetCurrent());
  Context::Scope context_scope(context);
  _context.Reset(Isolate::GetCurrent(), context);

  Local<String> source = String::NewFromUtf8(Isolate::GetCurrent(), "function __internal__json_stringify__tmp(obj) { return JSON.stringify(obj); }");
  Local<Script> script = Script::Compile(source);
  script->Run();
}

JsonStringifyer::~JsonStringifyer() {
  _context.Reset();
}

std::string JsonStringifyer::jsonStringify(Handle<Value> message) {
  // Create a new context for this (hrm, probably expensive...)
  HandleScope handle_scope(Isolate::GetCurrent());
  Local<Context> ctx = Local<Context>::New(Isolate::GetCurrent(), _context);
  Context::Scope context_scope(ctx);

  Handle<Object> global = ctx->Global();
  Handle<Value> fn_val = global->Get(String::NewFromUtf8(Isolate::GetCurrent(), "__internal__json_stringify__tmp"));
  Handle<Function> fn = Handle<Function>::Cast(fn_val);
  Handle<Value> args[1];
  args[0] = message;
  Handle<Value> result = fn->Call(global, 1, args);

  String::Utf8Value str(result);
  return *str;
}

JsonStringifyer *json_Stringer = NULL;

// A wrapper around the above class
std::string jsonStringify(Handle<Value> message) {
  if (!json_Stringer) {
    json_Stringer = new JsonStringifyer();
  }
  return json_Stringer->jsonStringify(message);
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
