#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>

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

//making struct for input thread args
typedef struct{
    int port;

} inputThreadArgs;

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

void *inputThread(void *arg){
    inputThreadArgs *args = (inputThreadArgs *)arg;//cast args to our struct
    int port = (*args).port; //get port number
    char url[1000];

    snprintf(url, 1000, "http://127.0.0.1:%d/NextInput", port );//builds URL

    //loop to keep checking the API for new Input
    while (1){
        CURL *curl = curl_easy_init(); //initialize curl

        if (curl){
            api_response response; 
            response.text = malloc(1);
            response.length = 0;

            response.text[0] = '\0';

            //set the URL
            curl_easy_setopt(curl, CURLOPT_URL, url);
            //use our saving_api_response function
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, saving_api_response);
            //pass curl response to  response
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);

            //do get request
            CURLcode cresponse = curl_easy_perform(curl);
            //check if request failed otherwise print next input
            if (cresponse != CURLE_OK){
                printf("error in curl: %s\n", curl_easy_strerror(cresponse));
            } else{
                printf("next input: %s\n", response.text);
            }

    
            
            //free memory
            free(response.text);
            curl_easy_cleanup(curl);

        }
        //stops program from spamming the API with requests.. does it every half second
        usleep(500000);
    }
    return NULL;
}

void startSim(int port) {
    CURL *curl = curl_easy_init();//initialize curl

    char url[1000];

    //build url
    snprintf(url, 1000, "http://127.0.0.1:%d/Simulation/start", port);

    //set url
    curl_easy_setopt(curl, CURLOPT_URL, url);

    //specify put method
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

    //excecute request
    CURLcode curlStatus = curl_easy_perform(curl);

    //check for failuer
    if (curlStatus != CURLE_OK) {
        printf("sim start failled: %s\n", curl_easy_strerror(curlStatus));
    } else {
        printf("Sim started\n");
    }

    curl_easy_cleanup(curl);
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

    startSim(port);//starts sim
    pthread_t inputThread1;

    inputThreadArgs inputArgs;
    inputArgs.port = port;

    //creates our input thread
    pthread_create(&inputThread1, NULL, inputThread, &inputArgs);
    pthread_join(inputThread1, NULL);
    
    //rest of code

    curl_global_cleanup();
    return 0;
}
