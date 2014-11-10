include ../Makefile.inc

TARGET = larson

$(TARGET): larson.cpp
	$(CXX) $(CXXFLAGS) -Wall -W -Werror larson.cpp -o $(TARGET) -lpthread

clean:
	rm -f $(TARGET)
