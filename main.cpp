/* 
 * File:   main.cpp
 * Author: Ivan
 * !!CLIENT!!
 * Here is a main file for network application. It uses znet lib: https://github.com/starwing/znet
 */
/* tell znet include all implement details into here */ 
#define ZN_IMPLEMENTATION
#include <cstdlib>
#include <stdio.h>
#include "znet.h"
#include <stdint.h>

#define MYDATA_BUFLEN 1024

char     addr[ZN_MAX_ADDRLEN] = "127.0.0.1";
unsigned port = 8081;

typedef struct MyData {
    char buffer[MYDATA_BUFLEN];
    int idx;
    int count;
} MyData;

using namespace std;
zn_State  *S;         /* the znet event loop handler */
zn_Tcp    *tcp;       /* znet tcp client handler     */
MyData    *data;      /* our user data pointer       */

/* we make a macro to compute a literal string's length, and put
 * length after string. */
#define send_string(str) str, sizeof(str)-1

//----------CALLBACK PROTOTYPES----------
/* function when a client is connected on a server. */
void on_connection(void *ud, zn_Tcp *tcp, unsigned err);
/* function when a tcp in client received something. */
void on_client_recv(void *ud, zn_Tcp *tcp, unsigned err, unsigned count);
/* function when a tcp in client sent something. */
void on_client_sent(void *ud, zn_Tcp *tcp, unsigned err, unsigned count);
//---------------------------------------

//this code is required to handle CTRL-C interrupts in console app in a propper way
//deinit and free all the resources
static void cleanup(void) {
    printf("exiting ... ");
    zn_close(S);
    printf("OK\n");
    printf("deinitialize ... ");
    zn_deinitialize();
    printf("OK\n");
}
#ifdef _WIN32
static int deinited = 0;
static BOOL WINAPI on_interrupted(DWORD dwCtrlEvent) {
    if (!deinited) {
        deinited = 1;
        /* windows ctrl handler is running at another thread */
        zn_post(S, (zn_PostHandler*)cleanup, NULL);
    }
    return TRUE;
}

static void register_interrupted(void) {
    SetConsoleCtrlHandler(on_interrupted, TRUE);
}
#else
#include <signal.h>

static void on_interrupted(int signum) {
    if (signum == SIGINT)
        cleanup();
}

static void register_interrupted(void) {
   struct sigaction act; 
   act.sa_flags = SA_RESETHAND;
   act.sa_handler = on_interrupted;
   sigaction(SIGINT, &act, NULL);
}
#endif
//CALLBACKS
/* the client connection callback: when you want to connect other
 * server, and it's done, this function will be called. */
void on_connection(void *ud, zn_Tcp *tcp, unsigned err) {
    MyData *data = (MyData*)ud;
    if (err != ZN_OK) { /* no lucky? let's try again. */
        /* we use ud to find out which time we tried. */
        fprintf(stderr, "[%p] client%d can not connect to server now: %s\n",
                tcp, data->idx, zn_strerror(err));
        if (++data->count < 10) {
            fprintf(stderr, "[%p client%d just try again (%d times)! :-/ \n",
                    tcp, data->idx, data->count);
            zn_connect(tcp, "127.0.0.1", 8080, on_connection, data);
        }
        else {
            fprintf(stderr, "[%p] client%d just give up to connect :-( \n",
                    tcp, data->idx);
            zn_deltcp(tcp);
            free(data);
        }
        return;
    }

    printf("[%p] client%d connected to server now!\n", tcp, data->idx);

    /* now we connect to the server, send something to server.
     * when send done, on_send() is called.  */
    /*zn_send(tcp, send_string("Hello world\n"), on_send, NULL);*/

    /* but, we want not just send one message, but five messages to
     * server. how we know which message we sent done? a idea is
     * setting many callback functions, but the better way is use a
     * context object to hold memories about how many message we sent.
     * */
    data->count = 0;
    zn_send(tcp, send_string("this is the first message from client!"),
            on_client_sent, data);

    /* and we raise a request to make znet check whether server send us
     * something ... */
    zn_recv(tcp, data->buffer, MYDATA_BUFLEN, on_client_recv, data);
}

void on_client_sent(void *ud, zn_Tcp *tcp, unsigned err, unsigned count) {
    MyData *data = (MyData*)ud;

    /* send work may error out, we first check the result code: */
    if (err != ZN_OK) {
        fprintf(stderr, "[%p] client%d meet problem when send something: %s\n",
                tcp, data->idx, zn_strerror(err));
        zn_deltcp(tcp); /* and we close connection. */
        free(data);
        return;
    }

    if (++data->count > 5) {
        printf("[%p] client%d ok, 5 messages sent and not even closing\n", tcp, data->idx);
        ++data->count = 0;
    }

    printf("[%p] client%d send messages success!\n", tcp, data->idx);
    printf("[%p] client%d send message%d to server ...\n",
            tcp, data->idx, data->count);
    //zn_send(tcp, send_string("message from client...\n"), on_client_sent, data);
}

void on_client_recv(void *ud, zn_Tcp *tcp, unsigned err, unsigned count) {
    MyData *data = (MyData*)ud; /* our data from zn_recv() */

    if (err != ZN_OK) {
        fprintf(stderr, "[%p] client%d meet error when receiving: %s",
                tcp, data->idx, zn_strerror(err));
        return;
    }

    fprintf(stderr, "[%p] client%d received from server: %.*s (%d bytes)\n",
            tcp, data->idx, (int)count, data->buffer, (int)count);
}
//---------
int main(int argc, char** argv) {
    printf("znet example: use %s engine.\n", zn_engine());
        /* first, we initialize znet global environment. we needn't do
     * this on *nix or Mac, because on Windows we should initialize
     * and free the WinSocks2 global service, the
     * zn_initialize()/zn_deinitialize() function is prepared for this
     * situation. */
    zn_initialize();
        /* after network environment is ready, we can create a new event
     * loop handler now. the zn_State object is the center object that
     * hold any resource znet uses, when everything overs, you should
     * call zn_close() to clean up all resources znet used. */
    S = zn_newstate();
    if (S == NULL) {
        fprintf(stderr, "create znet handler failured\n");
        return 2; /* error out */
    }
    /* Declare callback to release resources on exit (on CTRL+C) */
    register_interrupted();
    /* now connect to the server */
    data = (MyData*)malloc(sizeof(MyData));
    data->idx = 1;
    data->count = 0;
    tcp = zn_newtcp(S);
    zn_connect(tcp, addr, port, on_connection, data);
    printf("My Network Client.");
    uint8_t stopped = 0;
    zn_run(S, ZN_RUN_LOOP);
    while (stopped == 0) {
        //  https://stackoverflow.com/questions/1247989/how-do-you-allow-spaces-to-be-entered-using-scanf
        fgets(data->buffer, MYDATA_BUFLEN, stdin);
        if (strlen(data->buffer)>0 && (data->buffer[strlen (data->buffer) - 1] == '\n') ) {
            data->buffer[strlen (data->buffer) - 1] = '\0';
        }
        if (strlen(data->buffer) == 0) {continue;}
        zn_send(tcp, send_string(data->buffer), on_client_sent, data);
    }
    return 0;
}

