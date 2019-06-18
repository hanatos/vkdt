TARGET=librawinput.so
RAWSPEED_I=../../../../ext/rawspeed
RAWSPEED_L=../../../../built/ext/rawspeed
CXXFLAGS=-Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/ -I../.. -I../../.. -fPIC -g
LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lpugixml -lomp
#CXXFLAGS+=-fsanitize=address -O0
#LDFLAGS+=-fsanitize=address

$(TARGET): main.cc $(RAWSPEED_L)/librawspeed.a Makefile
	$(CXX) $(CXXFLAGS) main.cc -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
