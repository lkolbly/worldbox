build: messages.pb.o

%.pb.cc: %.proto
	protoc --cpp_out=. $<
	protoc --python_out=. $<

%.o: %.cc
	g++ -c -g -Wall -O0 -std=c++0x $< -o $@

clean:
	rm *.o *.pb.h *.py
