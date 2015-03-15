#ifndef MESSAGING_HXX
#define MESSAGING_HXX

#include<string>
#include<include/v8.h>

class MessageSubscriber {
private:
  bool _active;

public:
  MessageSubscriber();
  ~MessageSubscriber() noexcept;
  void Unsubscribe();
  void ReceiveMessage(v8::Handle<v8::Value> message);

  std::string channel_name;
  void (*_cb)(void *userdata, std::string channel, v8::Handle<v8::Value> message);
  void *_userdata;
};

void MsgAddSubscriber(MessageSubscriber subscriber);
void MsgBroadcast(std::string channel, v8::Handle<v8::Value> message);

#endif
