CXXFLAGS = -std=c++11 -Wall -Werror -O0 -g3
LDLIBS = -lboost_thread -lboost_system

all: example

%: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@
example: dynlock.hpp

clean:
	$(RM) example
