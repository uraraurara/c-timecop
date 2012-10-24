CC := gcc
all: timecop.c
	${CC} -Wall -D_GNU_SOURCE -fPIC -shared -o timecop.so $^ -ldl
