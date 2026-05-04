#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>

//establish queues size
#define QUEUE_SIZE 100

//creates peson data type with their name, sart floor and end floor
typedef struct{
    char person_id[100];
    int start_floor;
    int end_floor;
}Person;

// struct to hold elevator information
typedef struct {
    char name[1000];
    int lowest_floor;
    int highest_floor;
    int current_floor;
    int capacity;
} Elevator;

//data type to stor what elevator person is assigned to
typedef struct{
    Person person;
    char elevatorAssignment[100];

}ElevatorAssignment;

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

//creates data structure for Queue holding people waiting for elevators
typedef struct{
    Person items[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t notEmpty;
    pthread_cond_t notFull;
} ElevatorLine;

//queue for scheduler to put assignments in after decision is made, allows ouput to pull from it
typedef struct {
    ElevatorAssignment items[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t notEmpty;
    pthread_cond_t notFull;
} SchedulerQueue;

//making struct for input thread args
typedef struct{
    int port;
    ElevatorLine *elevatorLine;

} inputThreadArgs;

//structure for scheduler thread args
typedef struct{
    Elevator *elevators;
    int elevatorCount;
    ElevatorLine *elevatorLine;
    SchedulerQueue *schedulerQueue;
} SchedulerThreadArgs;

//structure for output thread args
typedef struct{
    int port;
    SchedulerQueue *schedulerQueue;
}OutputThreadArgs;

//function to push a person to the elevator line queue, used by input thread
void personPush(ElevatorLine *queue, Person person) 
{
    pthread_mutex_lock(&(*queue).mutex);

    while ((*queue).count == QUEUE_SIZE) 
    {
        pthread_cond_wait(&(*queue).notFull, &(*queue).mutex);
    }

    (*queue).items[(*queue).tail] = person;
    (*queue).tail = ((*queue).tail + 1) % QUEUE_SIZE;
    (*queue).count++;

    pthread_cond_signal(&(*queue).notEmpty);
    pthread_mutex_unlock(&(*queue).mutex);
}
//function to remove someone from the input queue, used by scheduler thread
Person personPop(ElevatorLine *queue) 
{
    pthread_mutex_lock(&(*queue).mutex);

    while ((*queue).count == 0) 
    {
        pthread_cond_wait(&(*queue).notEmpty, &(*queue).mutex);
    }

    Person person = (*queue).items[(*queue).head];
    (*queue).head = ((*queue).head + 1) % QUEUE_SIZE;
    (*queue).count--;

    pthread_cond_signal(&(*queue).notFull);
    pthread_mutex_unlock(&(*queue).mutex);

    return person;
}

//function to add to the scheduler queue, used by scheduler thread to pass data to output thread
void schedulerPush(SchedulerQueue *queue, ElevatorAssignment assignment)
{
    pthread_mutex_lock(&(*queue).mutex);

    while ((*queue).count == QUEUE_SIZE) 
    {
        pthread_cond_wait(&(*queue).notFull, &(*queue).mutex);
    }

    (*queue).items[(*queue).tail] = assignment;
    (*queue).tail = ((*queue).tail + 1) % QUEUE_SIZE;
    (*queue).count++;

    pthread_cond_signal(&(*queue).notEmpty);
    pthread_mutex_unlock(&(*queue).mutex);
}

//funciton to remove from scheduler queue, used by output thread
ElevatorAssignment schedulerPop(SchedulerQueue *queue) 
{
    pthread_mutex_lock(&(*queue).mutex);

    while ((*queue).count == 0) 
    {
        pthread_cond_wait(&(*queue).notEmpty, &(*queue).mutex);
    }

    ElevatorAssignment assignment = (*queue).items[(*queue).head];
    (*queue).head = ((*queue).head + 1) % QUEUE_SIZE;
    (*queue).count--;

    pthread_cond_signal(&(*queue).notFull);
    pthread_mutex_unlock(&(*queue).mutex);

    return assignment;
}

// function to parse text from /NextInput into my person struct
int parse_person_input(char *api_text, Person *person){

    // checking if the api sends back none and returning 0 becuase that means there is no person to store
    if (strcmp(api_text, "NONE") == 0){
        return 0;
}

    // ensuring the format is person_id start_floor and end_floor
    if (sscanf(api_text, "%[^ |] | %d | %d", person->person_id, &person->start_floor, &person->end_floor) == 3){
        return 1;
    }

    // returning -1 so show the api response did not match the expected format
    return -1;

}

//function to build the queue for input thread to put persons for scheduler thread
void initElevatorLine(ElevatorLine *queue) 
{
    (*queue).head = 0;
    (*queue).tail = 0;
    (*queue).count = 0;
    pthread_mutex_init(&(*queue).mutex, NULL);
    pthread_cond_init(&(*queue).notEmpty, NULL);
    pthread_cond_init(&(*queue).notFull, NULL);
}

//function to build queue for scheduler thread to put elevator assignments for output thread
void initSchedulerQueue(SchedulerQueue *queue) 
{
    (*queue).head = 0;
    (*queue).tail = 0;
    (*queue).count = 0;
    pthread_mutex_init(&(*queue).mutex, NULL);
    pthread_cond_init(&(*queue).notEmpty, NULL);
    pthread_cond_init(&(*queue).notFull, NULL);
}

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
    if (response->text == NULL) 
    {
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

// function to assign people to given elevator
void assign_elevator(int port, const char *person_id, const char *elevator_name) 
{
    CURL *curl = curl_easy_init();
    if (!curl) 
    {
        printf("Curl initialization failed for some reason\n");
        return;
    }

    char url[1000];
    
    // PUT /AddPersonToElevator/<personID>/<elevatorID>
    snprintf(url, 1000, "http://127.0.0.1:%d/AddPersonToElevator/%s/%s", port, person_id, elevator_name); //build URL

    curl_easy_setopt(curl, CURLOPT_URL, url); //set URL
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); //specify PUT

    // perform request
    CURLcode curlStatus = curl_easy_perform(curl);

    if (curlStatus != CURLE_OK) //if failed
    {
        printf("\nFailed to assign person %s to elevator: %s\n", person_id, elevator_name);
    } else //if successful
    {
        printf("\nSuccessfully requested to assign person %s to elevator: %s\n", person_id, elevator_name);
    }

    curl_easy_cleanup(curl);
}


//thread to take input from the API and pass to Scheduler thread
void *inputThread(void *arg)
{
    inputThreadArgs *args = (inputThreadArgs *)arg;//cast args to our struct
    int port = (*args).port; //get port number
    char url[1000];

    snprintf(url, 1000, "http://127.0.0.1:%d/NextInput", port );//builds URL

    //loop to keep checking the API for new Input
    while (1)
    {
        CURL *curl = curl_easy_init(); //initialize curl

        if (curl)
        {
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
            if (cresponse != CURLE_OK)
            {
                printf("error in curl: %s\n", curl_easy_strerror(cresponse));
            } else
            {
                printf("next input: %s\n", response.text);
                Person next_person;
                int parse_status = parse_person_input(response.text, &next_person);

                // if parsing was successful it will return 1
                if (parse_status == 1) 
                {
                    printf("%s wants to go from floor %d to %d\n", next_person.person_id, next_person.start_floor, next_person.end_floor);
                    /*
                    //assign person to elevator. hardcoded for now but we will need to call scheduling thread here
                    assign_elevator(port, next_person.person_id, "HotelBayA");
                    */
                    //push person to queue for scheduler to take from
                    personPush((*args).elevatorLine, next_person);
                } 
                //if parse_status is 0 it means the API sent NONE so we do nothing
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

//thread to take data from input thread and assign an elevator
void *schedulerThread(void *arg)
{
    SchedulerThreadArgs *args = (SchedulerThreadArgs *)arg;

    while(1)
    {
        Person person = personPop((*args).elevatorLine);

        ElevatorAssignment assignment;
        assignment.person = person;

        strcpy(assignment.elevatorAssignment, (*args).elevators[0].name);
        schedulerPush((*args).schedulerQueue, assignment);
    }

    return NULL;
    
}

//thread to take assignments from scheduler thread and output them
void *outputThread(void *arg){
        OutputThreadArgs *args = (OutputThreadArgs *)arg;

    while(1)
    {
        ElevatorAssignment assignment = schedulerPop((*args).schedulerQueue);

        assign_elevator((*args).port, assignment.person.person_id, assignment.elevatorAssignment);
    }
    return NULL;

}

//function to start simylation
void startSim(int port) 
{
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

    //check for failure
    if (curlStatus != CURLE_OK) 
    {
        printf("sim start failled: %s\n", curl_easy_strerror(curlStatus));
    } else 
    {
        printf("Sim started\n");
    }

    curl_easy_cleanup(curl);
}


int main(int argc, char *argv[]) 
{
    if (argc != 3) 
    {
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

    ElevatorLine elevatorLine;
    SchedulerQueue schedulerQueue;

    initElevatorLine(&elevatorLine);//create person input queue
    initSchedulerQueue(&schedulerQueue);//create elevator assignment queue

    pthread_t inputThread1;
    pthread_t schedulerThread1;
    pthread_t outputThread1;

    //pass input thread arguments
    inputThreadArgs inputArgs;
    inputArgs.port = port;
    inputArgs.elevatorLine = &elevatorLine;

    //pass scheduler thread arguments
    SchedulerThreadArgs schedulerArgs;
    schedulerArgs.elevators = building_elevators;
    schedulerArgs.elevatorCount = num_elevators;
    schedulerArgs.elevatorLine = &elevatorLine;
    schedulerArgs.schedulerQueue = &schedulerQueue;

    //pass output thread arguments
    OutputThreadArgs outputArgs;
    outputArgs.port = port;
    outputArgs.schedulerQueue = &schedulerQueue;

    //creates our input, scheduler, and output threads
    pthread_create(&inputThread1, NULL, inputThread, &inputArgs);
    pthread_create(&schedulerThread1, NULL, schedulerThread, &schedulerArgs);
    pthread_create(&outputThread1, NULL, outputThread, &outputArgs);

    pthread_join(inputThread1, NULL);
    pthread_join(schedulerThread1, NULL);
    pthread_join(outputThread1, NULL);

    curl_global_cleanup();
    return 0;
}
