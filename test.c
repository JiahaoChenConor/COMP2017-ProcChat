#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>



void set_path(char* folder, char* identifier, char* file_name, char* path, int type){
    int i = 0;
    int j = 0;
    for (j = 0; folder[j] != '\0'; j++){
        path[i++] = folder[j];
    }

    path[i++] = '/';

    if (identifier != NULL){
        for (j = 0; identifier[j] != '\0'; j++){
            path[i++] = identifier[j];
        }

        path[i++] = '_';

        if (type == 0){
            path[i++] = 'R';
            path[i++] = 'D';
        }else{
            path[i++] = 'W';
            path[i++] = 'R';
        }

        path[i] = '\0';
    }

    else if (file_name != NULL){
        for (j = 0; file_name[j] != '\0'; j++){
            path[i++] = file_name[j];
        }
        path[i] = '\0';
    }
   

}


struct Client
{
    char identifier[256];
    char domain[256];
    char path_pipe_RD[517];
    char path_pipe_WR[517];
};

void set_path_RD_WR(struct Client * client){
    set_path(client -> domain, client -> identifier, NULL, client -> path_pipe_RD, 0);
    set_path(client -> domain, client -> identifier, NULL, client -> path_pipe_WR, 1);
}


void connection(struct Client * client_ptr){
    char instruction[2048];
    memset(instruction, '\0', sizeof(instruction));
    instruction[0] = 0;
    instruction[1] = 0;

    strcpy(instruction + 2, client_ptr -> identifier);
    strcpy(instruction + 2 + 256, client_ptr -> domain);

    int gevent = open("gevent",O_WRONLY); 
    int write_res = write(gevent, instruction, 2048);
    close(gevent);

    if (write_res == 2048){
        printf("STATE {(Connected), identifier: %s, domain: %s}\n", client_ptr->identifier, client_ptr->domain); 
    }
}


void disconnect(struct Client *client_ptr){
    char instruction[2048];
    memset(instruction, '\0', sizeof(instruction));
    instruction[0] = 7;
    instruction[1] = 0;

    int fd_WR = open(client_ptr -> path_pipe_WR, O_WRONLY);
    int ret = write(fd_WR, instruction, 2048);
    close(fd_WR);
    if (ret == 2048){
        printf("STATE {(Disconnect) domain: %s, identifier: %s}\n", client_ptr->domain, client_ptr->identifier);
    }


}

int pipe_exists(struct Client * client){
    int WR_exists = 0;
    int RD_exists = 0;
    DIR *dirp;
    struct dirent *dp;
    dirp = opendir(client -> domain);
    if (dirp == NULL){
        return 0;
    }
    while ((dp = readdir(dirp)) != NULL)
    {
        char *name_of_pipes = dp->d_name;
        
        if (dp->d_type == DT_FIFO){
         
            int same = 1;
            int i = 0;
            for (i = 0; *(name_of_pipes + i) != '_'; i++){
                if (*(name_of_pipes + i) != (client -> identifier)[i]){
                    
                    same = 0;
                }
            }
         
            if (same){
                if (*(name_of_pipes + i + 1) == 'R' && *(name_of_pipes + i + 2) == 'D'){
                    RD_exists = 1;
                }
                else if (*(name_of_pipes + i + 1) == 'W' && *(name_of_pipes + i + 2) == 'R'){
                    WR_exists = 1;
                }
            }
        }
    }
    (void) closedir(dirp);
    if (RD_exists && WR_exists){
        return 1;
    }else{
        return 0;
    }
}

void SAY(struct Client * client_ptr, char * message){
    char instruction[2048];
    memset(instruction, '\0', sizeof(instruction));
    instruction[0] = 1;
    instruction[1] = 0;

    strcpy(instruction + 2, message);
    int fd_WR = open(client_ptr -> path_pipe_WR, O_WRONLY);
    int ret = write(fd_WR, instruction, 2048);
    close(fd_WR);
    if (ret == 2048){
        printf("SEND [(SAY) %s in domain %s SAY %s]\n", client_ptr->identifier, client_ptr->domain, message);
    }

}

void SAYCONT(struct Client * client_ptr, char * message, unsigned char termination){
    char instruction[2048];
    memset(instruction, '\0', sizeof(instruction));
    instruction[0] = 1;
    instruction[1] = 0;
    instruction[2047] = (char) termination;

    strcpy(instruction + 2, message);
    int fd_WR = open(client_ptr -> path_pipe_WR, O_WRONLY);
    int ret = write(fd_WR, instruction, 2048);
    close(fd_WR);
    if (ret == 2048){
        printf("SEND [(SAYCONT) %s in domain %s SAYCONT %s with termination %d]\n", client_ptr->identifier, client_ptr->domain, message, (unsigned char) termination);
    }
}

void PONG(struct Client * client_ptr){
    char instruction[2048];
    memset(instruction, '\0', sizeof(instruction));
    instruction[0] = 6;
    instruction[1] = 0;

    int fd_WR = open(client_ptr -> path_pipe_WR, O_WRONLY);
    int ret = write(fd_WR, instruction, 2048);
    close(fd_WR);
    if (ret == 2048){
        printf("SEND [(PONG) Send PONG to response PING]\n");
    }

}

void RECEIVE(struct Client * client_ptr){
    char received[2048];
    char identifier[256];
    char message_RECEIVE[1790];

    char message_RECVCONT[1789];
    char termination[1];

    memset(identifier, '\0', sizeof(identifier));
    memset(message_RECEIVE, '\0', sizeof(message_RECEIVE));
    memset(message_RECVCONT, '\0', sizeof(message_RECVCONT));
    memset(termination, '\0', sizeof(termination));
    
    
    int fd_RD = open(client_ptr -> path_pipe_RD, O_RDONLY);
    int ret = read(fd_RD, received, 2048);
    
    close(fd_RD);
    
    
    if (ret == 2048){
        if (received[0] == 3 && received[1] == 0){
            strcpy(identifier, received + 2);
            strcpy(message_RECEIVE, received + 2 + 256);
            printf("GTE <(RECEIVE) This is %s, from identifier: %s, message: %s>\n", client_ptr -> identifier,identifier, message_RECEIVE);
        }
        else if (received[0] == 4 && received[1] == 0){
            strcpy(identifier, received + 2);
            strcpy(message_RECVCONT, received + 2 + 256);
            strcpy(termination, received + 2 + 256 + 1789);
            printf("GET <(RECVCONT) This is %s, from identifier: %s, message: %s, termination: %d>\n", client_ptr -> identifier, identifier, message_RECVCONT, (unsigned char)termination[0]);
        }else if (received[0] == 5 && received[1] == 0){
            printf("GET <(PING)>\n");
        }    
    }
    
    
    
    
}


void test_gevent(){
    printf("### Test 1: Gevent ###\n");
    DIR *dirp;
    struct dirent *dp;
    dirp = opendir(".");
    int exists_gevent = 0;
    while ((dp = readdir(dirp)) != NULL)
    {
         char *name_of_pipes = dp->d_name;
        
        if (dp->d_type == DT_FIFO){
            if (strcmp(name_of_pipes, "gevent") == 0){
                exists_gevent = 1;
            }
        }

    }
    (void) closedir(dirp);
    if (exists_gevent){
        printf("--- Create pipe 'gevent' successfully\n");
    }

    sleep(1);
    printf("\n");
    
}

// This disconnect will delete the folder since there is no more pipes in this folder(domain)
void test_disconnect_1(){
    printf("### Test 2: DISCONNECT all clients in one domain ####\n");
    struct Client client1;
    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");
    set_path_RD_WR(&client1);

    connection(&client1);
    sleep(2);

    if (access("comp2017", W_OK) == 0){
        printf("--- Create domain 'comp2017' successfully\n");
    }
    
    if (pipe_exists(&client1)){
        printf("--- Create pipes of client1 successfully\n");
    }

    disconnect(&client1);

    if (access("comp2017", W_OK) != 0 && !pipe_exists(&client1)){
        printf("--- Disconnect client1 successfully\n");
    }
   
    sleep(1);
    printf("\n");

}

// This disconnect will not delete the folder since there is other pipes in this folder(domain)
void test_disconnect_2(){
    printf("### Test 3: DISCONNECT one of the clients in one domain ###\n");
    struct Client client1;
    struct Client client2;
    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");

    strcpy(client2.domain, "comp2017");
    strcpy(client2.identifier, "client2");


    set_path_RD_WR(&client1);
    set_path_RD_WR(&client2);
    connection(&client1);
    connection(&client2);

    sleep(2);
    if (access("comp2017", W_OK) == 0){
        printf("--- Create domain 'comp2017' successfully\n");
    }
    
    if (pipe_exists(&client1) && pipe_exists(&client2)){
        printf("--- Create pipes of client1 successfully\n");
    }

    sleep(2);
    disconnect(&client1);
    sleep(1);
    if (access("comp2017", W_OK) == 0 && pipe_exists(&client2) && !pipe_exists(&client1)){
        printf("--- Disconnect client1 successfully (client2 still Connected)\n");
    }

    disconnect(&client2);

    sleep(1);
    printf("\n");
}


// 2 clients in same domain
void test_say_receive1(){
    printf("### Test 4: SEND and RECEIVE (two clients in the same domain) ###\n");
    struct Client client1;
    struct Client client2;

    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");

    strcpy(client2.domain, "comp2017");
    strcpy(client2.identifier, "client2");

    set_path_RD_WR(&client1);
    set_path_RD_WR(&client2);

    connection(&client1);
    connection(&client2);
    sleep(2);

    SAY(&client1, "hello");
    RECEIVE(&client2);
    disconnect(&client1);
    sleep(1);
    disconnect(&client2);


    sleep(1);
    printf("\n");
}

// 3 clients in same domain
void test_say_receive2(){
    printf("### Test 5: SEND and RECEIVE ###\n");
    struct Client client1;
    struct Client client2;
    struct Client client3;

    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");

    strcpy(client2.domain, "comp2017");
    strcpy(client2.identifier, "client2");

    strcpy(client3.domain, "comp2017");
    strcpy(client3.identifier, "client3");

    set_path_RD_WR(&client1);
    set_path_RD_WR(&client2);
    set_path_RD_WR(&client3);

    connection(&client1);
    connection(&client2);
    connection(&client3);

    sleep(2);

    SAY(&client1, "hello");
    RECEIVE(&client2);
    RECEIVE(&client3);
    disconnect(&client1);
    sleep(1);
    disconnect(&client2);
    sleep(1);
    disconnect(&client3);

    sleep(1);
    printf("\n");

}
// clients in different domain
void test_say_receive3(){
    printf("### Test 6: SAY and RECEIVE (clients in different domains)\n");
    struct Client client_A1;
    struct Client client_A2;

    struct Client client_B1;
    struct Client client_B2;

    strcpy(client_A1.domain, "comp2017_groupA");
    strcpy(client_A1.identifier, "client_A1");

    strcpy(client_A2.domain, "comp2017_groupA");
    strcpy(client_A2.identifier, "client_A2");

    strcpy(client_B1.domain, "comp2017_groupB");
    strcpy(client_B1.identifier, "client_B1");

    strcpy(client_B2.domain, "comp2017_groupB");
    strcpy(client_B2.identifier, "client_B2");

    set_path_RD_WR(&client_A1);
    set_path_RD_WR(&client_A2);
    set_path_RD_WR(&client_B1);
    set_path_RD_WR(&client_B2);

    connection(&client_A1);
    connection(&client_A2);
    connection(&client_B1);
    connection(&client_B2);

    sleep(2);
    SAY(&client_A1, "Hello, groupA");
    RECEIVE(&client_A2);
    printf("(The first message of clients in groub_B received is PING rather than 'Hello, groupA')\n ");
    RECEIVE(&client_B1);
    RECEIVE(&client_B2);

    disconnect(&client_A1);
    sleep(1);
    disconnect(&client_A2);
    sleep(1);
    disconnect(&client_B1);
    sleep(1);
    disconnect(&client_B2);
    sleep(1);
    printf("\n");
}

void test_saycont_recvcont1(){
    printf("### Test 7: simple SENDCONT and RECEIVECONT ###\n");
    struct Client client1;
    struct Client client2;

    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");

    strcpy(client2.domain, "comp2017");
    strcpy(client2.identifier, "client2");

    set_path_RD_WR(&client1);
    set_path_RD_WR(&client2);

    connection(&client1);
    connection(&client2);
    sleep(2);

    SAYCONT(&client1, "hello", 255);
    RECEIVE(&client2);
    disconnect(&client1);
    sleep(1);
    disconnect(&client2);

    sleep(1);
    printf("\n");

}


// continuous message
void test_saycont_recvcont2(){

    printf("### Test 8: SENDCONT and RECEIVECONT (continuous messages)###\n");
    struct Client client1;
    struct Client client2;

    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");

    strcpy(client2.domain, "comp2017");
    strcpy(client2.identifier, "client2");

    set_path_RD_WR(&client1);
    set_path_RD_WR(&client2);

    connection(&client1);
    connection(&client2);
    sleep(2);

    SAYCONT(&client1, "hello", 0);
    RECEIVE(&client2);

    SAYCONT(&client1, "how", 0);
    RECEIVE(&client2);

    SAYCONT(&client1, "are", 0);
    RECEIVE(&client2);

    SAYCONT(&client1, "you", 255);
    RECEIVE(&client2);

    disconnect(&client1);
    sleep(1);
    disconnect(&client2);

    sleep(1);
    printf("\n");
    
}

void test_say_receive_mix_saycont_recvcont(){
    printf("### Test 9: SAY/RECVIVE mix with SAYCONT/RECVCONT ###\n");
    struct Client client1;
    struct Client client2;

    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");

    strcpy(client2.domain, "comp2017");
    strcpy(client2.identifier, "client2");

    set_path_RD_WR(&client1);
    set_path_RD_WR(&client2);

    connection(&client1);
    connection(&client2);
    sleep(2);

    SAYCONT(&client1, "hello", 0);
    RECEIVE(&client2);

    SAYCONT(&client1, "how", 0);
    RECEIVE(&client2);

    SAYCONT(&client1, "are", 0);
    RECEIVE(&client2);

    SAY(&client1, "XXXXX");
    RECEIVE(&client2);

    SAYCONT(&client1, "you", 255);
    RECEIVE(&client2);

    disconnect(&client1);
    sleep(2);
    disconnect(&client2);


    sleep(1);
    printf("\n");
}


// client don't response in 
void test_ping(){
    printf("### Test 10: PING ###\n");
    struct Client client1;
    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");
    set_path_RD_WR(&client1);

    connection(&client1);
    sleep(15);
    
    RECEIVE(&client1);

    sleep(16);
    if (pipe_exists(&client1) == 0 && access("comp2017_4", W_OK) != 0){
        printf("--- Client never response, disconnect successfully\n");
    }
    

    sleep(1);
    printf("\n");
}

void test_ping_pong(){
    printf("### Test 11: PING PONG ###\n");
    struct Client client1;
    strcpy(client1.domain, "comp2017");
    strcpy(client1.identifier, "client1");
    set_path_RD_WR(&client1);

    connection(&client1);
    sleep(15);
    
    RECEIVE(&client1);
    sleep(1);
    PONG(&client1);


    sleep(15);
    RECEIVE(&client1);
    sleep(2);
    

    if (pipe_exists(&client1) && access("comp2017", W_OK) == 0){
        printf("--- Client response with PONG, successfully\n");
    }
   

    disconnect(&client1);
    printf("\n");
}



int main(){
    test_gevent();
    test_disconnect_1();
    test_disconnect_2();
    test_say_receive1();
    test_say_receive2();
    test_say_receive3();
    test_saycont_recvcont1();
    test_saycont_recvcont2();
    test_say_receive_mix_saycont_recvcont();
    test_ping();
    test_ping_pong();
    
}
