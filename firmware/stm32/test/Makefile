# Проверка на собствения код на компютъра (не е част от firmware-а).
# Нужни са gcc и python3.
CC ?= gcc
CFLAGS = -std=c11 -O2 -g -Wall -Wextra -Wshadow -Wno-pointer-to-int-cast -Imock -I../Core/Inc

all: stepgen_test
	./stepgen_test
	python3 check_pins.py

stepgen_test: stepgen_test.c ../Core/Src/stepgen.c ../Core/Inc/stepgen.h mock/main.h
	$(CC) $(CFLAGS) -o $@ stepgen_test.c -lm

clean:
	rm -f stepgen_test

.PHONY: all clean
