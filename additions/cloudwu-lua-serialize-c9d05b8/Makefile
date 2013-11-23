all : serialize

serialize : serialize.c
	gcc -Wall -g -o $@.dll --shared $^ -I/usr/local/include -L/usr/local/bin -llua52


mingw:
	gcc -Wall -o serialize.dll --shared -I../../src/lua -L../../src/lua -llua5.1

linux:
	gcc -m32 -Wall -o serialize.so --shared -I../../src/lua -L../../src/lua -llua
