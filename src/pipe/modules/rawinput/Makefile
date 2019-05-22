TARGET=librawinput.so
CXX=clang++
RAWSPEED_I=../../../../ext/rawspeed
RAWSPEED_L=../../../../built/ext/rawspeed
CXXFLAGS=-Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/ -I../.. -I../../.. -fPIC
LDFLAGS=-L$(RAWSPEED_L) -lrawspeed

$(TARGET): main.cc $(RAWSPEED_L)/librawspeed.a Makefile
	$(CXX) $(CXXFLAGS) main.cc -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
