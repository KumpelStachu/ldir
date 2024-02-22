NAME=ldir

CC=cc
FLAGS=-Wall -Werror -Wextra -pedantic -std=c99 -O3
DEBUG_FLAGS=-fsanitize=address
SRC=*.c
LIBS=
RM =rm -rf
PREFIX=/usr/local

.PHONY: all clean run debug install

all: $(NAME)

$(NAME): $(SRC)
	$(CC) $(FLAGS) $(LIBS) $(SRC) -o $(NAME)

clean:
	$(RM) $(NAME)

run: all
	./$(NAME)

debug: clean
	$(CC) $(FLAGS) $(DEBUG_FLAGS) $(LIBS) $(SRC) -o $(NAME)

install: all
	mkdir -p $(PREFIX)/bin
	cp $(NAME) $(PREFIX)/bin/$(NAME)
