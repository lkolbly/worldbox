V8ROOT=/home/lane/Downloads/v8/v8
V8LIB=$(V8ROOT)/out/x64.release/lib.target/

OBJTARGETS = vec3.o entity.o main.o messaging.o net.o util.o
PROTOTARGETS = net/messages.pb.o

build: proto $(OBJTARGETS)
	g++ -g -Wall -O0 -std=c++0x -L$(V8LIB) $(OBJTARGETS) $(PROTOTARGETS) $(V8ROOT)/out/x64.release/obj.target/tools/gyp/libv8_libplatform.a -lrt -lv8 -licuuc -licui18n -lBulletDynamics -lBulletCollision -lLinearMath /usr/local/lib/libprotobuf.so -pthread

proto:
	make -C net/

%.o: %.cxx
	g++ -c -g -Wall -O0 -std=c++0x -I/home/lane/Downloads/rapidjson/include -I/usr/include/bullet/ -I$(V8ROOT) $< -o $@

clean:
	make -C net/ clean
	rm *.o
