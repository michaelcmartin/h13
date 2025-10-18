CFLAGS=`sdl2-config --cflags` `pkg-config epoxy --cflags` -Iinclude -O2
LIBS=`sdl2-config --libs` `pkg-config epoxy --libs`

checkerboard: demo/checkerboard.o src/h13_oglwin.o
	$(CC) $(LDFLAGS) -o $@ demo/checkerboard.o src/h13_oglwin.o $(LIBS)
