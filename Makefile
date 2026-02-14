CFLAGS=`sdl2-config --cflags` -Iinclude -O2
LIBS=`sdl2-config --libs`
H13OBJS= src/h13_oglwin.o src/gl.o

checkerboard: demo/checkerboard.o $(H13OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(H13OBJS) $(LIBS)

clean:
	rm -f demo/*.o src/*.o
