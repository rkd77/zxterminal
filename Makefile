all: tmx85 tmx80 tmx64 spc64_c spc64_m timex.tap spectrum.tap

tmx85: tmx85.c
	$(CC) -o tmx85 -DMAX_X=85 -O3 $(CFLAGS) -I/usr/include/ncurses -lvterm -lpthread  `pkg-config --cflags --libs glib-2.0` tmx85.c -ggdb3

tmx80: tmx85.c
	$(CC) -o tmx80 -DMAX_X=80 -O3 $(CFLAGS) -I/usr/include/ncurses -lvterm -lpthread  `pkg-config --cflags --libs glib-2.0` tmx85.c -ggdb3

tmx64: tmx64.c
	$(CC) -o tmx64 -O3 $(CFLAGS) -I/usr/include/ncurses -lvterm -lpthread `pkg-config --cflags --libs glib-2.0` tmx64.c -ggdb3

spc64_c: spc64_c.c
	$(CC) -o spc64_c -O3 $(CFLAGS) -I/usr/include/ncurses  -lvterm -lpthread `pkg-config --cflags --libs glib-2.0` spc64_c.c -ggdb3

spc64_m: spc64_m.c
	$(CC) -o spc64_m $(CFLAGS) -I/usr/include/ncurses  -lvterm -lpthread `pkg-config --cflags --libs glib-2.0` spc64_m.c -ggdb3

timex.tap: spectrum.c
	zcc +ts2068 -DTIMEX -vn -startup=2 -lndos -llibsocket -lim2 -create-app -o timex.bin spectrum.c

spectrum.tap: spectrum.c
	zcc +zx -vn  -lndos -llibsocket -lim2 -create-app -o spectrum.bin spectrum.c
