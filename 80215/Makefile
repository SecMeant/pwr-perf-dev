ifeq ($(OS),Windows_NT)
	CC=g++
	RM=del
else
	CC=i686-w64-mingw32-g++
	RM=rm
endif

all:
	$(CC) blue_test.cpp -lws2_32 -lBthprops -std=c++17

clean:
	$(RM) *.obj *.exe

syntax:
	$(CC) blue_test.cpp -fsyntax-only
