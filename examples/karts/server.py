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

        msg = messages_pb2.GlobalWorldSettings()
        msg.physics_gravity.x = 0.0
        msg.physics_gravity.y = -10.0
        msg.physics_gravity.z = 0.0
        self.sendMessage(0x02, msg)

        msg = messages_pb2.SpawnEntity()
        msg.cfg_filename = "examples/karts/game_assets/terrain.json"
        self.sendMessage(0x0201, msg)

        # This is a list of everyone who's asked us to make an entity,
        # but we're still waiting to give them said entity.
        self.expecting_clients = []
        self.karts = {}
        self.projectiles = {}
        self.clients = [] # All clients
        self.expecting_projectiles = []

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
        if msg_type == 0x0201:
            msg = messages_pb2.EntitySpawned()
            msg.ParseFromString(body)
            if "terrain" == msg.entity_type:
                pass
            if "kart" == msg.entity_type:
                expecting = self.expecting_clients.pop(0)
                expecting.gotEntity(str(msg.id))
                self.karts[str(msg.id)] = expecting

                msg2 = messages_pb2.GetEntityLocation()
                msg2.id = msg.id
                self.sendMessage(0x0202, msg2)
            if "projectile" == msg.entity_type:
                msg2 = messages_pb2.GetEntityLocation()
                msg2.id = msg.id
                self.sendMessage(0x0202, msg2)
                self.projectiles[str(msg.id)] = True

        if msg_type == 0x0202:
            msg = messages_pb2.EntityLocation()
            msg.ParseFromString(body)
            if str(msg.id) in self.karts:
                self.karts[str(msg.id)].updateKartLocation(msg)
                for c in self.clients:
                    c.updateOtherKartLocation(msg)
            else: # It must be a projectile
                for c in self.clients:
                    c.setProjectileLocation(msg)
        if msg_type == 0x0204:
            msg = messages_pb2.RemoveEntity()
            msg.ParseFromString(body)
            if str(msg.entity_id) in self.karts:
                self.karts[str(msg.entity_id)].kartDestroyed()
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
        msg.start_position.y = 10.0
        msg.start_position.z = random.random()*20.0-10.0
        self.sendMessage(0x0201, msg)
        self.expecting_clients.append(cb)
        pass

    def removeEntity(self, eid):
        msg = messages_pb2.RemoveEntity()
        print eid
        print int(eid)
        msg.entity_id = int(eid)
        self.sendMessage(0x0204, msg)

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

    def protobufpos2json(self, kloc):
        return {"id": str(kloc.id), "position": {"x": kloc.position.x, "y": kloc.position.y, "z": kloc.position.z}, "rotation": {"x": kloc.rotation.x, "y": kloc.rotation.y, "z": kloc.rotation.z}}

    def write_obj(self, obj):
        try:
            self.write_message(json.dumps(obj))
        except:
            pass

    def updateKartLocation(self, kloc):
        # Forward that on...
        kart_location = self.protobufpos2json(kloc)
        kart_location["type"] = "my_loc"
        self.write_obj(kart_location)
        pass

    def updateOtherKartLocation(self, kloc):
        obj = self.protobufpos2json(kloc)
        obj["type"] = "other_loc"
        self.write_obj(obj)
        pass

    def setProjectileLocation(self, kloc):
        obj = self.protobufpos2json(kloc)
        obj["type"] = "proj_loc"
        self.write_obj(obj)

    def on_message(self, message):
        message = json.loads(message)
        if message["type"] == "controls":
            msg = messages_pb2.MsgBroadcast()
            msg.channel = "controls_%s"%self.eid
            msg.json = json.dumps(message["controls"])
            worldbox.sendMessage(0x0101, msg)
            #print "Sending controls message: %s"%msg.json
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
