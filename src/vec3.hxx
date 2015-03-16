#ifndef VEC3_HXX
#define VEC3_HXX 1

#include<include/v8.h>
#include<bullet/btBulletDynamicsCommon.h>

class Vec3 {
private:
  double _x, _y, _z;

public:
  static v8::Handle<v8::ObjectTemplate> MakeTemplate(v8::Isolate *isolate);
  static v8::Handle<v8::Object> Wrap(Vec3 *v, v8::Isolate *isolate);

  static void GetXYZ(v8::Local<v8::Name> name,
		     const v8::PropertyCallbackInfo<v8::Value>& info);
  static void SetXYZ(v8::Local<v8::Name> name, v8::Local<v8::Value> newval,
		     const v8::PropertyCallbackInfo<void>& info);
  static void ToString(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ToStringAccessor(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info);

  void SetXYZ(double x, double y, double z);
  double GetX() { return _x; };
  double GetY() { return _y; };
  double GetZ() { return _z; };
  void Set(btVector3 v);
  btVector3 ToBtVec();

  Vec3 operator*(double scalar);
};

#endif
