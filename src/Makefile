V8ROOT=/home/lane/Downloads/v8/v8
V8LIB=$(V8ROOT)/out/x64.release/lib.target/
#LIBS=$(V8LIB)/out/x64.release/obj.target/tools/gyp/libv8_base.a $(V8LIB)/out/x64.release/obj.target/tools/gyp/libv8_libbase.a $(V8LIB)/out/x64.release/obj.target/tools/gyp/libv8_snapshot.a $(V8LIB)/out/x64.release/obj.target/tools/gyp/libv8_libplatform.a $(V8LIB)/out/x64.release/obj.target/third_party/icu/libicuuc.a $(V8LIB)/out/x64.release/obj.target/third_party/icu/libicui18n.a $(V8LIB)/out/x64.release/obj.target/third_party/icu/libicudata.a

#g++ -std=c++0x -I. -I/home/lane/Downloads/v8/v8 main.cxx -o hello_world -Wl,--start-group $(LIBS) -Wl,--end-group -lrt -pthread
build:
	g++ -g -Wall -O0 -std=c++0x -I$(V8ROOT) -L$(V8LIB) main.cxx $(V8ROOT)/out/x64.release/obj.target/tools/gyp/libv8_libplatform.a -lrt -lv8 -licuuc -licui18n -pthread
