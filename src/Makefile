V8ROOT=/home/lane/Downloads/v8/v8
V8LIB=$(V8ROOT)/out/x64.release/lib.target/

OBJTARGETS = vec3.o entity.o main.o

build: $(OBJTARGETS)
	g++ -g -Wall -O0 -std=c++0x -I$(V8ROOT) -L$(V8LIB) $(OBJTARGETS) $(V8ROOT)/out/x64.release/obj.target/tools/gyp/libv8_libplatform.a -lrt -lv8 -licuuc -licui18n -pthread

%.o: %.cxx
	g++ -c -g -Wall -O0 -std=c++0x -I$(V8ROOT) -L$(V8LIB) $< -o $@

clean:
	rm *.o
