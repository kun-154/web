src = $(wildcard ./src/*.c)
obj = $(patsubst ./src/%.c, ./obj/%.o, ${src})

CC = gcc
CFLAGS = -I ./inc -lpthread -g

ALL:a.out

a.out: ${obj}
	${CC} $^ -o $@ $(CFLAGS)

${obj}: ./obj/%.o:./src/%.c
	${CC} -c $< -o $@ ${CFLAGS} 

clean:
	-rm -rf ${obj} a.out

.PHONY: clean ALL
