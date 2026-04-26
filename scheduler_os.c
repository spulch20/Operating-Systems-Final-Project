#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>

// struct to hold elevator information
typedef struct {
    char name[10];
    int lowest_floor;
    int highest_floor;
    int current_floor;
    int capacity;
} Elevator;

//parse the .bldg file and put each elevator in the array of elevators, return # of elevators parsed
int parse_bldg_file(const char *filename, Elevator *elevators) 
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening .bldg file\n");
        return -1;
    }

    int count = 0;
    char line[256];

    // read file
    while (fgets(line, sizeof(line), file))
    {
        //extract fields
        if (sscanf(line, "%[^\t]\t%d\t%d\t%d\t%d", elevators[count].name, &elevators[count].lowest_floor, &elevators[count].highest_floor, &elevators[count].current_floor, &elevators[count].capacity) == 5) 
        // the above line reads a string, skips a tab, and then reads four integers separated by tabs
        {
            count++;
        } 
    }

    fclose(file);
    return count;
}
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Error, 2 arguments required: bldg_file and port\n");
        exit(1);
    }

    char *bldg_file = argv[1];
    int port = atoi(argv[2]);

    Elevator building_elevators[10];
    int num_elevators = parse_bldg_file(bldg_file, building_elevators);
    if (num_elevators == -1) exit(1);

    curl_global_init(CURL_GLOBAL_ALL);

    //rest of code

    curl_global_cleanup();
    return 0;
}