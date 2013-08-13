APP = spots
CFL = -c -Wall -std=c++11 -masm=intel -O3
LFL = -s -static -mwindows
SRC = $(APP).cpp
OBJ = $(SRC:.cpp=.o)
LIB = -lgdi32

all: $(OBJ)
	g++ $(LFL) -o $(APP).exe $(OBJ) $(LIB)

%.o: %.cpp
	g++ $(CFL) $*.cpp -o $@

clean:
	@del $(OBJ) > NUL 2>&1
	@del $(APP).exe > NUL 2>&1
