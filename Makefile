BackItUp: BackItUp.o
	gcc -o BackItUp BackItUp.o -Wall -Werror

BackItUp.o: BackItUp.c
	gcc -c BackItUp.c

run:
	./BackItUp

clean:
	rm *.o BackItUp