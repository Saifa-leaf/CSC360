#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>

struct train {
    int id;
    char direction[1];
    int load_time;
    int cross_time;
};

struct station {
    struct station* prev;
    struct train* cart;
    struct station* next;
};

struct station* FIRST = NULL;

#define BILLION 1000000000L;

//Keep track of number of train that finish loading
int READY = 0;
//last train direction, 0 = east, 1 = west
int LASTTRAINDIR = 1;
// count number of train the go east/west back to back
int GOWEST = 0;
int GOEAST = 0;
// cond of allLoad
int signal_all = 0;
// cond of add/remove from list
int cond = 0;

// muntex and cov
pthread_mutex_t mutex_list = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_main = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_load = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_add = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_remove = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_allLoad  = PTHREAD_COND_INITIALIZER;

//number of all the train
int TRAIN_COUNT = 0;

/*
Purpose: read each line from a file and stored them in 2d array
*/
void read_file(char *filename, char needed[1024][10]) {
    FILE* file = fopen(filename,"r");
    if (file == NULL) {
        printf("file can't be open");
        exit(0);
    }

    
    char line[100]; 
    int i = 0;
    while ((fgets(line, 100, file)) != NULL) {
        strcpy(needed[i], line);
        i++;
    }
    TRAIN_COUNT = i;
    fclose(file);
    i++;
    strcpy(needed[i], "done");
}

/*
Purpose: Convert each information in 2d array into train struct
*/
void converter(char needed[1024][10], struct train trainArr[TRAIN_COUNT]) {
    int i = 0;
    char dir[1];
    int lt, ct;

    while (i != TRAIN_COUNT) {
        sscanf(needed[i], "%c %d %d", dir, &lt, &ct);

        trainArr[i].id = i;
        strcpy(trainArr[i].direction, dir);
        trainArr[i].load_time = lt;
        trainArr[i].cross_time = ct;
        i++;
    }

    //printf("%s %d %d:\n", trainArr[0].direction, trainArr[0].load_time, trainArr[0].cross_time);
}

/*
Purpose: after a train is loaded, this function is call to 
    print the time the train take to load follow by train id
    and direction.
*/
void print_load(struct train* aTrain, double accum) {
    int hour = 0;
    int min = 0;
    double usec = accum;
    char dir[2]; strcpy(dir, aTrain->direction);

    if (usec < 600) {
        printf("00:00:%04.1f ", usec);
    } else if (usec >= 600 && usec < 35999){
        min = (int)usec/600;
        usec = (double)(((int)usec)%600);

        printf("00:%02d:%04.1f ", min, usec);
    } else {
        hour = (int)usec/36000;
        min = (int)usec%36000;
        min = min/600;
        usec = (double)(((int)usec)%600);

        printf("%02d:%02d:%04.1f ", hour, min, usec);
    }

    if (strcmp(dir, "e") == 0 || strcmp(dir, "E") == 0) {
        printf("Train %2d is ready to go East\n", aTrain->id);
    } else {
        printf("Train %2d is ready to go West\n", aTrain->id);
    }
}

/*
Purpose: add train that finish loading to global doubly linked list
*/
void addToList(struct train* aTrain) {
    pthread_mutex_lock(&mutex_list);
    READY++;
    while (cond == 1) {
        pthread_cond_wait(&cond_add, &mutex_list);
    }
    struct station *cur = FIRST;
    struct station *new = (struct station*)malloc(sizeof(struct station));
    new->cart = aTrain;


    if (FIRST == NULL) {
        FIRST = new;
        FIRST->next = NULL;
        FIRST->prev = NULL;
    } else {
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = new;
        new->prev = cur;
        new->next = NULL;
    }

    READY--;
    if (READY > 0 || FIRST == NULL) {
        pthread_cond_signal(&cond_add);
        pthread_mutex_unlock(&mutex_list);
    } else {
        cond = 1;
        pthread_cond_signal(&cond_remove);
        pthread_mutex_unlock(&mutex_list);
    }
}

/*
Purpose: remove train that finish running on the main track from 
    global doubly linked list
*/
void removeFromList(struct station* cur) {

    if (cur->prev == NULL && cur->next == NULL) {
        free(cur);
        FIRST = NULL;
    } else if (cur->prev == NULL) {
        FIRST = cur->next;
        FIRST->prev = NULL;
        free(cur);
    } else if (cur->next == NULL){
        cur->prev->next = NULL;
        cur->prev = NULL;
        free(cur);
    } else {
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
        free(cur);
    }

}

/*
Purpose: print time it take during crossing of the train on the main track
*/
void printTime(struct timespec start) {
    struct timespec stop;
    double accum;

    if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) {
        perror( "clock gettime" );
        exit( EXIT_FAILURE );
    }

    accum = ((double) stop.tv_sec - start.tv_sec ) + ((double) stop.tv_nsec - start.tv_nsec ) /BILLION;


    int hour = 0;
    int min = 0;
    double usec = accum;

    if (usec < 600) {
        printf("00:00:%04.1f ", usec);
    } else if (usec >= 600 && usec < 35999){
        min = (int)usec/600;
        usec = (double)(((int)usec)%600);

        printf("00:%02d:%04.1f ", min, usec);
    } else {
        hour = (int)usec/36000;
        min = (int)usec%36000;
        min = min/600;
        usec = (double)(((int)usec)%600);

        printf("%02d:%02d:%04.1f ", hour, min, usec);
    }
}

/*
Purpose: simulate the train crossing east
*/
void goEast (struct train* e, struct timespec start) {

    printTime(start);
    printf ("Train %2d is ON the main track going East\n", e->id);
    usleep(e->cross_time *100000);

    printTime(start);
    printf ("Train %2d is OFF the main track after going East\n", e->id);

    GOEAST++;
    LASTTRAINDIR = 0;
}

/*
Purpose: simulate the train crossing east
*/
void goWest(struct train* w, struct timespec start) {

    printTime(start);
    printf ("Train %2d is ON the main track going West\n", w->id);
    usleep(w->cross_time *100000);

    printTime(start);
    printf ("Train %2d is OFF the main track after going West\n", w->id);

    GOWEST++;
    LASTTRAINDIR = 1;
}

/*
Purpose: look through the global list and determine which train run next
*/
void dispatcher(struct timespec start) {
    pthread_mutex_lock(&mutex_main);
    while (cond == 0) {
        pthread_cond_wait(&cond_remove, &mutex_main);
    }
    struct train* Etrain = NULL;
    struct train* Wtrain = NULL;
    struct train* etrain = NULL;
    struct train* wtrain = NULL;
    struct station* Estation = NULL;
    struct station* Wstation = NULL;
    struct station* estation = NULL;
    struct station* wstation = NULL;
    struct station* cur = FIRST;
    char dir[2];
    int check = 0;
    int curID = cur->cart->id;


    while (cur != NULL && check <= 2) {
        if (cur->cart->id == curID) {
            check++;
        }
        strcpy(dir, cur->cart->direction);
        if (strcmp(dir, "E") == 0 && Etrain == NULL) {
            Etrain = cur->cart;
            Estation = cur;
        } else if (strcmp(dir, "W") == 0 && Wtrain == NULL) {
            Wtrain = cur->cart;
            Wstation = cur;
        } else if (strcmp(dir, "w") == 0 && wtrain == NULL) {
            wtrain = cur->cart;
            wstation = cur;
        } else if (strcmp(dir, "e") == 0 && etrain == NULL) {
            etrain = cur->cart;
            estation = cur;
        } else if (strcmp(dir, "E") == 0 && Etrain != NULL) {
            if (Etrain->id > cur->cart->id) {
                Etrain = cur->cart;
                Estation = cur;
            }
        } else if (strcmp(dir, "W") == 0 && Wtrain != NULL) {
            if (Wtrain->id > cur->cart->id) {
                Wtrain = cur->cart;
                Wstation = cur;
            }
        } else if (strcmp(dir, "w") == 0 && wtrain != NULL) {
            if (wtrain->id > cur->cart->id) {
                wtrain = cur->cart;
                wstation = cur;
            }
        } else if (strcmp(dir, "e") == 0 && etrain != NULL) {
            if (etrain->id > cur->cart->id) {
                etrain = cur->cart;
                estation = cur;
            }
        } 
        cur = cur->next;
    }

    if (GOEAST >= 4 && (Wtrain != NULL || wtrain != NULL)) {
        GOEAST = 0;
        if (Wtrain != NULL) {
            goWest(Wtrain, start);
            removeFromList(Wstation);
        } else {
            goWest(wtrain, start);
            removeFromList(wstation);
        }
    } else if (GOWEST >= 4 && (Etrain != NULL || etrain != NULL)) {
        GOWEST = 0;
        if (Etrain != NULL) {
            goEast(Etrain, start);
            removeFromList(Estation);
        } else {
            goEast(etrain, start);
            removeFromList(estation);
        }
    } else if (Etrain != NULL && Wtrain != NULL) {
        if (LASTTRAINDIR == 0) {
            goWest(Wtrain, start);
            removeFromList(Wstation);
        } else {
            goEast(Etrain, start);
            removeFromList(Estation);
        }
    } else if (Etrain != NULL) {
        goEast(Etrain, start);
        removeFromList(Estation);
    } else if (Wtrain != NULL) {
        goWest(Wtrain, start);
        removeFromList(Wstation);
    } else if (etrain != NULL && wtrain != NULL) {
        if (LASTTRAINDIR == 0) {
            goWest(wtrain, start);
            removeFromList(wstation);
        } else {
            goEast(etrain, start);
            removeFromList(estation);
        }
    } else if (etrain != NULL) {
        goEast(etrain, start);
        removeFromList(estation);
    } else if (wtrain != NULL) {
        goWest(wtrain, start);
        removeFromList(wstation);
    }


    if (READY > 0 || FIRST == NULL) {
        cond = 0;
        pthread_cond_signal(&cond_add);
        pthread_mutex_unlock(&mutex_main);
    } else {
        pthread_cond_signal(&cond_remove);
        pthread_mutex_unlock(&mutex_main);
    }
}

/*
Purpose: include all function a pthread needed to execute.

*/
void *loading_dock(void* arg) {
    struct train* aTrain = arg;
	struct timespec start, stop;
    double accum;

    pthread_mutex_lock( &mutex_load );
    pthread_cond_wait( &cond_allLoad, &mutex_load );
    pthread_mutex_unlock( &mutex_load );
    
    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) {
        perror( "clock gettime" );
        exit( EXIT_FAILURE );
    }

    usleep(aTrain->load_time *100000);

    if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) {
        perror( "clock gettime" );
        exit( EXIT_FAILURE );
    }
    
    accum = ((double) stop.tv_sec - start.tv_sec ) + ((double) stop.tv_nsec - start.tv_nsec ) /BILLION;

    print_load(aTrain, accum);

    addToList(aTrain);
    dispatcher(start);

    pthread_exit(NULL);
}

/*
Purpose: create each pthread, signal pthread synchronous in loading_dock(),
    wait for all pthread to join.
*/
int main (int argc, char *argv[]) {
    char needed[1024][10];
    char *filename = argv[1];

    if (filename == NULL || argc != 2) {
        printf("to run this: ./p2 *insert txt file as an argument*\n");
        exit(0);
    }

    read_file(filename, needed);
    struct train trainArr[TRAIN_COUNT];
    converter(needed, trainArr);

    pthread_t thread_id[TRAIN_COUNT];
    for(int i=0; i < TRAIN_COUNT; i++) {
        pthread_create(&thread_id[i], NULL, &loading_dock, (void*)&trainArr[i]);
    }

    sleep(1);
    pthread_cond_broadcast(&cond_allLoad);

    for(int j=0; j < TRAIN_COUNT; j++) {
        pthread_join(thread_id[j], NULL); 
    }
    exit(0);

}

