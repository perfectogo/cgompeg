#include <unistd.h>
#include <stdio.h>

int read_file_from_pipe(int fd) {


    printf("C received: %d", fd);
    
    FILE *output = fopen("output.mp4", "w");
    if (!output) {
        perror("Failed to open output file");
        return 1;
    }

    char buffer[1024];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, output); // Write data to file
        //printf("C received: %s", buffer);
    }

    if (bytes_read < 0) {
        perror("Error reading from pipe");
        return 1;
    }

    fclose(output);

    return 0;
}