PORT=51782
CFLAGS= -DPORT=\$(PORT) -g -Wall

battle: battle.c
	gcc $(CFLAGS) -o battle battle.c