#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>

// struct to hold elevator information
typedef struct {
    char name[1000];
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
// struct to store the text we get back from the api
typedef struct {
    char *text;
    size_t length;
} api_response;

// defining a function for the curl library to call whenever text is sent back from the api
size_t saving_api_response(void *api_text, size_t byte_size, size_t item_count, void *response_pointer)
{
    // finding how many bytes curl got from the api
    size_t total_bytes = byte_size * item_count;

    // making our pointer point to the api_response struct from above
    api_response *response = (api_response *)response_pointer;

    // making more space so we can continue to store more text from the api
    response->text = realloc(response->text, response->length + total_bytes + 1);

    // if NULL is found, memory allocation failed, so return 0 to tell curl to stop
    if (response->text == NULL) {
        return 0;
    }

    // copying the newest api response after the text already stored
    memcpy(response->text + response->length, api_text, total_bytes);

    // updating the length after new api text is added
    response->length += total_bytes;

    // add '\0' so C knows where the api response string ends
    response->text[response->length] = '\0';

    // letting curl know how many bytes were saved
    return total_bytes;
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
