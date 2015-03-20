#ifndef UTIL_HXX
#define UTIL_HXX 1

#include <include/v8.h>

std::string jsonStringify(v8::Handle<v8::Value> message);
std::string readFile(const char *pathname);

#endif
