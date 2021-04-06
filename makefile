CC = gcc
REMOVE = rm

aubatch: aubatch.c
	$(CC) -o Job Job.c
	$(CC) -o aubatch -lpthread aubatch.c
clean:
	rm *.o aubatch

