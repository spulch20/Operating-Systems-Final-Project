scheduler_os:
	gcc scheduler_os.c -o scheduler_os -lcurl -lpthread

clean:
	rm -f scheduler_os