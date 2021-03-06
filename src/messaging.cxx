#include<string>
#include<vector>
#include<include/v8.h>

#include "messaging.hxx"

using namespace v8;

std::vector<MessageSubscriber> message_Subscribers;

MessageSubscriber::MessageSubscriber() {
  _active = true;
  channel_name = "";
}

MessageSubscriber::~MessageSubscriber() {
}

void MessageSubscriber::Unsubscribe() {
  _active = false;
}

void MessageSubscriber::ReceiveMessage(Handle<Value> message) {
  if (_active)
    _cb(_userdata, channel_name, message);
}

void MsgBroadcast(std::string channel, Handle<Value> message) {
  for (std::vector<MessageSubscriber>::iterator it=message_Subscribers.begin(); it!=message_Subscribers.end(); ++it) {
    MessageSubscriber sub = *it;
    if (sub.channel_name.compare(channel) == 0 || channel.compare("") == 0) {
      // Pass along the message
      // Note: The callback cannot modify the message.
      sub.ReceiveMessage(message);
    }
  }
}

void MsgAddSubscriber(MessageSubscriber subscriber) {
  message_Subscribers.push_back(subscriber);
}
