APP = rt
CFL = -c -Wall -fopenmp -std=c++11 -O3
# -ffast-math -march=core2 -mfpmath=both
LFL = -s -static -mwindows
SRC = $(APP).cpp
OBJ = $(SRC:.cpp=.o)
LIB = -lgdi32 -lgomp

all: $(OBJ)
	g++ $(LFL) -o $(APP).exe $(OBJ) $(LIB)

%.o: %.cpp
	g++ $(CFL) $*.cpp -o $@

clean:
	@del $(OBJ) > NUL 2>&1
	@del $(APP).exe > NUL 2>&1
