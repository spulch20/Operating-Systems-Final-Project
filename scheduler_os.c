#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

//function to read and print the .bldg file
void read_bldg(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        printf("Error opening .bldg file");
        exit(1);
    }

    printf("Elevator Configurations\n");
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    printf("\n");
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Error, 2 arguments required: bldg_file and port\n");
        exit(1);
    }

    char *bldg_file = argv[1];
    port = atoi(argv[2]);

    curl_global_init(CURL_GLOBAL_ALL);

    read_bldg(bldg_file);

    //rest of code

    curl_global_cleanup();
    return 0;
}