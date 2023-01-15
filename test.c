#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int pid = getpid();
    while(1) {
        printf("My PID is %d\n", pid);
    }
}