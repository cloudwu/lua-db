linux:
	gcc -fPIC -O2 -Wall -o database.so --shared lua-db.c hash.c -I/usr/local/include	

win32:
	gcc -O2 -Wall -march=i586 -o database.dll --shared lua-db.c hash.c -I/usr/local/include -L/usr/local/bin -llua52

test:
	gcc -O2 -Wall -o testmt -I/usr/local/include test.c -lpthread -llua -lm -ldl

hash:
	gcc -g -Wall -o testhash hash.c testhash.c -I/usr/local/include	-L/usr/local/bin -llua52