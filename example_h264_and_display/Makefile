# Makefile
TARGET = usbcam 

# CFLAG = -Wall -Werror -std=c99
#CFLAG = -Wall -std=c99 -O2
CFLAG = -Wall -O2
LIB = -pthread -lx264

${TARGET}: main.o camera.o encode.o queue.o display.o
	${CC} main.o camera.o encode.o display.o queue.o ${LIB} ${CFLAG} -o ${TARGET}

main.o: main.c config.h
	${CC} main.c ${CFLAG} -c -o main.o

camera.o: camera.c camera.h config.h
	${CC} camera.c ${CFLAG} -c -o camera.o

encode.o: encode.c encode.h config.h
	${CC} encode.c ${CFLAG} -c -o encode.o

queue.o: queue.c queue.h config.h
	${CC} queue.c ${CFLAG} -c -o queue.o

display.o: display.c display.h config.h
	${CC} display.c ${CFLAG} -c -o display.o

.PHONY: clean

clean:
	rm -f *.o ${TARGET}
