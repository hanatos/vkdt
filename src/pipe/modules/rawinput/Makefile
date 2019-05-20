TARGET=librawinput.so
CXX=clang++
RAWSPEED=../../../../ext/rawspeed
CXXFLAGS=-Wall -I$(RAWSPEED)/src/librawspeed/ -I$(RAWSPEED)/build/src/ -I$(RAWSPEED)/src/external/ -I../.. -I../../.. -fPIC
LDFLAGS=-L$(RAWSPEED)/build/ -lrawspeed

$(TARGET): main.cc
	$(CXX) $(CXXFLAGS) main.cc -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
