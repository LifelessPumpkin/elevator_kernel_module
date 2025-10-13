#include <unistd.h>
#include <string.h>

int main() {
    char message[] = "Hello\n";
    write(1,message,strlen(message));
    write(1,message,strlen(message));
    write(1,message,strlen(message));
    write(1,message,strlen(message));
    write(1,message,strlen(message));
    return 0;
}