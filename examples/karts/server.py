import random
import socket
import struct
import json
import binascii
import tornado.ioloop
import tornado.web
from tornado import websocket
import messages_pb2

io_loop = None

class WorldBox:
    def __init__(self, HOST="localhost", PORT=15538):
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
        self.socket = s

        msg = messages_pb2.Hello()
        msg.version = 1
        self.sendMessage(0x01, msg)

        msg = messages_pb2.MsgSubscribe()
        msg.channel = "kartCreated"
        self.sendMessage(0x0102, msg)

        msg = messages_pb2.MsgSubscribe()
        msg.channel = "kartLocation"
        self.sendMessage(0x0102, msg)

        msg = messages_pb2.MsgSubscribe()
        msg.channel = "kartDestroyed"
        self.sendMessage(0x0102, msg)

        msg = messages_pb2.SpawnEntity()
        msg.cfg_filename = "examples/karts/game_assets/terrain.json"
        self.sendMessage(0x0201, msg)

        # This is a list of everyone who's asked us to make an entity,
        # but we're still waiting to give them said entity.
        self.expecting_clients = []
        self.karts = {}
        self.clients = [] # All clients

        io_loop.add_handler(self.socket.fileno(), lambda a,b: self.ondata(a,b,None), io_loop.READ)

    def ondata(self, sock, fd, events):
        hdr = ""
        while len(hdr) < 4:
            hdr += self.socket.recv(4-len(hdr))
        #print binascii.hexlify(hdr)
        (msg_size, msg_type) = struct.unpack("!HH", hdr)
        #print "%X %X"%(msg_size,msg_type)
        body = ""
        while len(body) < msg_size:
            #print "receiving %i..."%(msg_size-len(body))
            body += self.socket.recv(msg_size-len(body))
        if msg_type == 0x0101:
            msg = messages_pb2.MsgBroadcast()
            msg.ParseFromString(body)
            #print msg.channel
            obj = json.loads(msg.json)
            if msg.channel == "kartCreated":
                # Give this kart to the first guy
                expecting = self.expecting_clients.pop(0)
                expecting.gotEntity(obj["id"])
                self.karts[obj["id"]] = expecting
            elif msg.channel == "kartLocation":
                if obj["id"] in self.karts:
                    self.karts[obj["id"]].updateKartLocation(obj)
                for c in self.clients:
                    c.updateOtherKartLocation(obj)
            elif msg.channel == "kartDestroyed":
                if obj["id"] in self.karts:
                    self.karts[obj["id"]].kartDestroyed()
            #print "Got message: %s"%msg.json
        pass

    def sendMessage(self, msgType, msg):
        body = msg.SerializeToString()
        msg = struct.pack("!HH", len(body), msgType)
        msg += body
        self.socket.sendall(msg)
        pass

    def addClient(self, cb):
        msg = messages_pb2.SpawnEntity()
        msg.cfg_filename = "examples/karts/game_assets/kart.json"
        msg.start_position.x = random.random()*20.0-10.0
        msg.start_position.y = random.random()*20.0-10.0
        msg.start_position.z = random.random()*20.0-10.0
        self.sendMessage(0x0201, msg)
        self.expecting_clients.append(cb)
        pass

    def removeEntity(self, eid):
        msg = messages_pb2.MsgBroadcast()
        msg.channel = "kill_%s"%eid
        msg.json = ""
        self.sendMessage(0x0101, msg)

worldbox = None

class GameWebSocket(websocket.WebSocketHandler):
    def open(self):
        print "Client connected. Creating entity..."
        self.eid = -1
        worldbox.addClient(self)
        worldbox.clients.append(self)

    def gotEntity(self, eid):
        print "Entity created w/ eid=%s"%eid
        self.eid = eid
        self.write_message(json.dumps({"ourKartId": eid}))
        pass

    def kartDestroyed(self):
        print "We lost, going again..."
        worldbox.addClient(self)
        pass

    def updateKartLocation(self, kart_location):
        # Forward that on...
        kart_location["type"] = "my_loc"
        self.write_message(json.dumps(kart_location))
        pass

    def updateOtherKartLocation(self, obj):
        obj["type"] = "other_loc"
        self.write_message(json.dumps(obj))
        pass

    def on_message(self, message):
        message = json.loads(message)
        if message["type"] == "controls":
            msg = messages_pb2.MsgBroadcast()
            msg.channel = "controls_%s"%self.eid
            msg.json = json.dumps(message["controls"])
            worldbox.sendMessage(0x0101, msg)
            print "Sending controls message: %s"%msg.json
            pass

    def on_close(self):
        print "Closing websocket. Delete the kart."
        worldbox.removeEntity(self.eid)
        worldbox.clients.remove(self)
        pass

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write("Hello, world")

class EchoWebSocket(websocket.WebSocketHandler):
    def open(self):
        print "WebSocket opened"

    def on_message(self, message):
        self.write_message(u"You said: " + message)

    def on_close(self):
        print "WebSocket closed"

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/echows",EchoWebSocket),
    (r"/ws",GameWebSocket),
])

if __name__ == "__main__":
    application.listen(8888)
    io_loop = tornado.ioloop.IOLoop.instance()
    worldbox = WorldBox()
    io_loop.start()
