CC = gcc
CXXFLAGS = -c -g

OUT_FILE = tp

OBJECTS = tp.o
SRC = .

$(OUT_FILE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(OUT_FILE) -lX11 -lXi

tp.o: $(SRC)/tp.c
	$(CC) $(CXXFLAGS) $(SRC)/tp.c
