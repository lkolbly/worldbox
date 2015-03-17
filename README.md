# WorldBox

WorldBox is an entity-centric game engine that uses JavaScript.

What does that mean? It means that you define entities and how they interact, and WorldBox does the heavy lifting for you - physics, game loop, interaction with the outside world. You build entities using JavaScript. Then WorldBox runs these entities, and you talk to WorldBox over the network to get the status of the entities - position, controls, things like that.

Right now we don't really have any of that, but that's the goal.

# Installing

Requirements are:

- v8 (the JavaScript engine)
- Bullet Physics
- rapidjson
- Protobuf (version 2.6.0)
- Make, g++, etc.

On Ubuntu, you can install bullet through apt-get. V8 is harder to install - I followed the instructions at https://code.google.com/p/v8-wiki/wiki/UsingGit and https://code.google.com/p/v8-wiki/wiki/BuildingWithGYP

Rapidjson is fairly straight forward, it can be obtained from https://github.com/miloyip/rapidjson

Protobuf can be installed through apt-get on ubuntu, but that gives you an old version. To get the latest either use Vervet or compile from source.

You may need to edit src/Makefile to match your setup. V8ROOT is where I had my V8 source tree.

Then, run make from the root directory. That'll build an a.out executable for you.

# Demos

The demos require Python with Tornado. I'm using python 2.7.6 with tornado 3.2.

You will need to point a webserver at the example's web folder to access the GUI - I pointed an instance of Apache at examples/karts/web.

Compile the worldbox as shown above. Run worldbox in the root directory (the same directory as stdlib.js). Then cd to examples/karts and run:
$ python server.py

This will start a websockets server on port 8888. Hit the appropriate URL (that you set above, when you were pointing your web server at the demo folder), and that will bring up the demo UI.

# TODO

A lot of things need to be done. This is in no particular order.

- A sample game. This is to scope out everything that needs to be done in terms of the API.
- Entities should have some way of querying the environment around them. In other words, asking questions like "am I running into anyone?" or "who is within 20 meters of me?"
  - On a related note, message passing should be improved. We should be able to target messages to specific entities, which implies that entities should have some sort of ID system (this ID can be a number that's returned when spawnEntity() is called). Also, message passing should (potentially) exist with a finite speed of light - that is, it takes time for the messages to travel.
- Collision meshes & entity stuffs. Entities should be able to specify a collision mesh (either as a triangle mesh or using a set of primitives). Also, entities should be able to spawn new entities at a location.
- Terrain. Right now it's fixed, but it should be turned into a sort of special-case entity. If entities can spawn new entities, then the terrain could be infinite and procedurally generated.
- A better controller system. Right now controllers exist separately, each with setTarget() and update() commands (see stdlib.js). I think it would be nice for game developers to be able to do things like chain these without having to write the code in the middle (e.g. chain two PID controllers). Also, some sort of auto-tuning feature or setPosition feature would be handy for PID controllers, since they can be hard to tune.
