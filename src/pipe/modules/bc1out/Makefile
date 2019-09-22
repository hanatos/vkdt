TARGET=libbc1out.so
SQUISH_I=
SQUISH_L=-lsquish
CXXFLAGS=-Wall $(SQUISH_I) -I../.. -I../../.. -fPIC -g
LDFLAGS=$(SQUISH_L) -lz

$(TARGET): main.cc Makefile
	$(CXX) $(CXXFLAGS) main.cc -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
