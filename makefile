all:server client1 client2 client3 client4
server: server.c
	gcc server.c  -lpthread -o server
client1:client1.c
	gcc client1.c -lm -lpthread -o client1 
client2:client2.c
	gcc client2.c -lm -lpthread -o client2 
client3:client3.c
	gcc client3.c -lm -lrt -lpthread -o client3
client4:client4.c
	gcc client4.c -o client4

clean:
	rm -f client1
	rm -f client2
	rm -f client3
	rm -f client4
	rm -f server
