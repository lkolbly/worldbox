import random
import socket
import struct, binascii
import messages_pb2

HOST = "localhost"
PORT = 15538
s = None
for res in socket.getaddrinfo(HOST, PORT, socket.AF_UNSPEC, socket.SOCK_STREAM):
    af, socktype, proto, canonname, sa = res
    try:
        s = socket.socket(af, socktype, proto)
    except socket.error as msg:
        s = None
        continue
    try:
        s.connect(sa)
    except socket.error as msg:
        s.close()
        s = None
        continue
    break

def sendMessage(msgType, msg):
    body = msg.SerializeToString()
    msg = struct.pack("!HH", len(body), msgType)
    msg += body
    s.sendall(msg)
    pass

hello_msg = messages_pb2.Hello()
hello_msg.version = 123
sendMessage(0x01, hello_msg)

msg = messages_pb2.MsgSubscribe()
msg.channel = "test_msgs"
sendMessage(0x0102, msg)

msg = messages_pb2.MsgBroadcast()
msg.channel = "test_msgs"
msg.msg = "Message from a client!"
sendMessage(0x0101, msg)

# Spawn an entity to play with
msg = messages_pb2.SpawnEntity()
msg.type = "test2.json"
#msg.start_position = messages_pb2.Vec3()
msg.start_position.x = random.random()*20.0-10.0
msg.start_position.y = random.random()*20.0-10.0
msg.start_position.z = random.random()*20.0-10.0
sendMessage(0x0201, msg)

# Now receive a message
while 1:
    hdr = ""
    while len(hdr) < 4:
        hdr += s.recv(4-len(hdr))
    print binascii.hexlify(hdr)
    (msg_size, msg_type) = struct.unpack("!HH", hdr)
    print "%X %X"%(msg_size,msg_type)
    body = ""
    while len(body) < msg_size:
        print "receiving %i..."%(msg_size-len(body))
        body += s.recv(msg_size-len(body))
    if msg_type == 0x0101:
        msg = messages_pb2.MsgBroadcast()
        msg.ParseFromString(body)
        print "Got message: %s"%msg.msg

s.close()
