all: f___system.c
	mkdir -p test_dir && gcc -Wall f___system.c `pkg-config fuse3 --cflags --libs` -o f___system

run:
	./f___system test_dir

clean:
	fusermount -u test_dir 
	rm -rf test_dir
	rm f___system