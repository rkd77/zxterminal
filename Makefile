all: tmx85 tmx64 spc64_c spc64_m timex.tap spectrum_c.tap spectrum_m.tap

tmx85: tmx85.c
	gcc -o tmx85 -DMAX_X=85 -I/usr/include/ncurses -lvterm -lpthread  `pkg-config --cflags --libs glib-2.0` tmx85.c -ggdb3

tmx80: tmx85.c
	gcc -o tmx85 -DMAX_X=80 -I/usr/include/ncurses -lvterm -lpthread  `pkg-config --cflags --libs glib-2.0` tmx85.c -ggdb3

tmx64: tmx64.c
	gcc -o tmx64 -I/usr/include/ncurses -lvterm -lpthread `pkg-config --cflags --libs glib-2.0` tmx64.c -ggdb3

spc64_c: spc64_c.c
	gcc -o spc64_c -I/usr/include/ncurses  -lvterm -lpthread `pkg-config --cflags --libs glib-2.0` spc64_c.c -ggdb3

spc64_m: spc64_m.c
	gcc -o spc64_m -I/usr/include/ncurses  -lvterm -lpthread `pkg-config --cflags --libs glib-2.0` spc64_m.c -ggdb3

timex.tap: timex.c
	zcc +ts2068 -vn -startup=2 -lndos -llibsocket -lim2 -create-app -o timex.bin timex.c

spectrum_c.tap: spectrum_c.c
	zcc +zx -vn  -lndos -llibsocket -lim2 -create-app -o spectrum_c.bin spectrum_c.c

spectrum_m.tap: spectrum_m.c
	zcc +zx -vn  -lndos -llibsocket -lim2 -create-app -o spectrum_m.bin spectrum_m.c
