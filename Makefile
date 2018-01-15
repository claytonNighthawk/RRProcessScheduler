CC = gcc
CFLAGS = -g -W -Wall -std=c11
OBJECTS = p1fxns.o iterator.o
AL_OBJECTS = arraylist.o 
BQ_OBJECTS = bqueue.o 
TEST_FILE_OBJS = cpubound iobound

main: run4

test: p1fxns.o test.c  
	$(CC) $(CFLAGS) -o $@ test.c p1fxns.o

uspsv1: $(OBJECTS) $(AL_OBJECTS) uspsv1.o $(TEST_FILE_OBJS)
	$(CC) $(CFLAGS) -o $@ uspsv1.o $(OBJECTS) $(AL_OBJECTS)

uspsv2: $(OBJECTS) $(AL_OBJECTS) uspsv2.o $(TEST_FILE_OBJS)
	$(CC) $(CFLAGS) -o $@ uspsv2.o $(OBJECTS) $(AL_OBJECTS)

uspsv3: $(OBJECTS) $(AL_OBJECTS) $(BQ_OBJECTS) uspsv3.o $(TEST_FILE_OBJS)
	$(CC) $(CFLAGS) -o $@ uspsv3.o $(OBJECTS) $(AL_OBJECTS) $(BQ_OBJECTS)

uspsv4: $(OBJECTS) $(AL_OBJECTS) $(BQ_OBJECTS) uspsv4.o $(TEST_FILE_OBJS)
	$(CC) $(CFLAGS) -o $@ uspsv4.o $(OBJECTS) $(AL_OBJECTS) $(BQ_OBJECTS)

run1:
	./uspsv1 --quantum=500 workfile2.txt

run2: uspsv2
	./uspsv2 workfile2.txt --quantum=500

run3: uspsv3
	./uspsv3 workfile2.txt --quantum=500

run4: uspsv4
	./uspsv4 workfile3.txt --quantum=500

valgrindtest: uspsv4
	valgrind --leak-check=full ./uspsv3 --quantum=1000 workfile3.txt

# Remove all objects and programs 
clean: 
	$(RM) *.o a.out test iobound cpubound uspsv1 uspsv2 uspsv3 uspsv4