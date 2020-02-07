BUILD               = debug

CXX                 = g++
LD                  = g++
RM                  = rm

TARGET              = set-projector-power
SOURCES             = set-projector-power.cpp
OBJECTS             = $(SOURCES:.cpp=.o)

CFLAGS              = -std=gnu++17 -Wall -Wextra -pedantic
LFLAGS              = 
ifeq ($(BUILD),debug)
CFLAGS             += -D_DEBUG -g -Og
LFLAGS             += -g
else ifeq ($(BUILD),release)
CFLAGS             += -DNDEBUG -O3
LFLAGS             += -s
endif

all: $(TARGET)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $<

$(TARGET): $(OBJECTS)
	$(LD) $(LFLAGS) -o $@ $^

clean:
	-$(RM) $(TARGET) $(OBJECTS) 

## Dependencies

set-projector-power.o: set-projector-power.cpp
