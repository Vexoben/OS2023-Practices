#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


int main(){
    
    pid_t pid;
    // OPEN FILES
    int fd;
    fd = open("test.txt" , O_RDWR | O_CREAT | O_TRUNC);
    if (fd == -1) {
        fprintf(stderr, "fail on open %s\n", "test.txt");
		return -1;
    }
    //write 'hello fcntl!' to file
    write(fd, "hello fcntl", strlen("hello fcntl"));
    /* code */

    int dupl = fcntl(fd, F_DUPFD, 0);
    
    // DUPLICATE FD

    /* code */


    pid = fork();

    if(pid < 0){
        // FAILS
        printf("error in fork");
        return 1;
    }
    
    struct flock fl;

    if(pid > 0){
        printf("father process\n");
        char str[100];

        fl.l_type = F_WRLCK;
        fl.l_start = 0;
        fl.l_whence = SEEK_SET;
        fl.l_len = 0;
        fcntl(fd, F_SETLK, &fl);
        // PARENT PROCESS
        //set the lock

        write(fd, "b", strlen("b"));
        lseek(fd, 0, SEEK_SET);
        read(fd, str, 50);
        printf("%s\n", str);

        //append 'b'
        
        //unlock
        fl.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &fl);
        sleep(3);

        lseek(fd, 0, SEEK_SET);
        read(fd, str, 50);
        printf("%s\n", str);
        exit(0);

    } else {
        printf("child process\n");
        // CHILD PROCESS
        sleep(2);
        fl.l_type = F_WRLCK;
        fl.l_start = 0;
        fl.l_whence = SEEK_SET;
        fl.l_len = 0;
        fcntl(dupl, F_SETLK, &fl);
        //get the lock
        
        //append 'a'
        write(dupl, "a", strlen("a"));
        fl.l_type = F_UNLCK;
        fcntl(dupl, F_SETLK, &fl);

        exit(0);
    }
    close(fd);
    return 0;
}