CC = gcc
FLAGS = -Wall -Wdeprecated-non-prototype -Wunused-variable -Wunused-value -g

defrag: defrag.c defrag.h
	$(CC) -o defrag $(FLAGS) defrag.c

clean:
	rm defrag disk_defrag
