#include "server.h"

int state = 1;
/*
                       .-> +-------------+    send PING to client every 15s
                       |   | PING Handler| ------------+
                       |   +-------------+             |
         PONG(SIGUSR2) |        ^                      |
                       |        |   fork               | 
                       |        |                      | 
    +------+    fork   +------------------+    ————————+———RD———>   +-----------+
    |      | --------> | Client Handler A |                         | Client A  |
    |Server|           +-----------+------+    <————————————WR——    +-----------+_________
    |      |                       |                                                      |
    |      |                       `--------、                                            ｜
    |      |                                |                                             |
    |      |    fork.  +------------------+ |  ————————————RD———>   +----------+          |   
    |      | --------> | Client Handler B | `----、                 | Client B |          ｜ 
    +------+           +------------------+    <——+—————————WR——    +----------+          |   
        ^                                                                   |             |    
        |                                                                   |             |
        |___________________________________________________________________|<------------+
                                gevent


   ++ How can I avoid zombie process ?
   -- For the Client handler, when we receiver disconnect, send a signal1 to its parent process 'server'
        On the one hand, client handler use exit(0) to exit, on the other hand, when parent process receive signal1,
        it will use 'wait' to deal with exited child process


    -- For the ping handler
        Case 1: Client handler disconnect, first kill ping handler, then client handler will exit and his child process will become Orphan Process
        Case 2: Never receive pong, so ping handler send disconnect to client handler, and .....
*/


static void handler(int signo)
{   

    int   status;
    wait(&status);
   
}

static void handler_signal2(int signo)
{   
    state = 0;
}


int main() {
    
    // Create gevent
    int gevent_exist = access("gevent", F_OK);
    if (gevent_exist == -1){
        int ret = mkfifo("gevent", 0664);
        if (ret == -1){
            perror("Error in creating the gevent pipe.\n");
            return 1;
        }

    }
   

    while (1){
        char buffer[2048];
        // BLOCK
        int fd_gevent = open("gevent", O_RDONLY);
        if (fd_gevent == -1){
            perror("Error in open gevent pipe.\n");
            return 1;
        }
        // Read is non-blocking
        int nread = read(fd_gevent, buffer, sizeof(buffer));
        close(fd_gevent);
        
        if (nread == -1){
            perror("Read error\n");
            return 1;
        }else if (nread == 0){
            continue;
        }else if (nread == 2048){
            // Connect
            if (buffer[0] == CONNECT && buffer[1] == TYPE_PADDING){
                // Global process, listen SIGUSR1, and use handler to 'wait' child -> client handler
                signal(SIGUSR1, handler);
                int pid = fork();
                if (pid < 0){
                    perror("Fork fail!\n");
                    return 1;
                }else if (pid == 0){
                    // Child: client handler
                    // The last byte is reserved for '\0'
                    char domain[257];
                    char identifier[257];
                    
                    str_copy_extra_termination(identifier, buffer + 2, 257);
                    str_copy_extra_termination(domain, buffer + 2 + 256, 257);
                    
                    creatFolder(domain);

                    // path has three parts, <domain>/<identifier>_RD
                    //                        256 + 1 + 256 +     3  + 1 (\0)
                    char path_pipe_RD[517];
                    char path_pipe_WR[517];
                    set_path(domain, identifier, NULL, path_pipe_RD, 0);
                    set_path(domain, identifier, NULL, path_pipe_WR, 1);

                    int ret_RD_pipe = mkfifo(path_pipe_RD, 0666);
                    int ret_WR_pipe = mkfifo(path_pipe_WR, 0666);
                    if (ret_RD_pipe == -1 || ret_WR_pipe == -1){
                        perror("Error when creating RD/WR\n");
                    }

                    // Global variable 'state' which is 1
                    // Create a grandchild called PING handler
                    // Set the 'state' into 1 when sending ping
                    // When received pong(in client handler -- child process), sending signal2 to grandchild to set 'state' into 0
                    // Check the state before sending the second ping and subsequent pings

                    // Time line 
                    //          0  ---------  15    -----------        30       -----------   45   -----------   60  ...  ---------- N*(15)
                    //                      send ping     pong    check state,send ping        
                    // STATE:   0            0 -> 1       1->0        0?      0->1  
                    //                         
                    //                     ping  (check pong)   ping (check pong)  ping  (check pong)
                    int pid_ping = fork();
                    if (pid_ping < 0){
                        perror("fork ping error\n");
                        return 1;
                    }else if (pid_ping == 0){
                        // This is grandchild process to send ping.
                        // This signal is used for listening PONG from client handler 
                        // Through this we can change the state according to the pong from client handler
                        signal(SIGUSR2, handler_signal2);
                    
                        time_t start = time(NULL);
                        time_t now = time(NULL);

                        while (1){
                            now = time(NULL);
                            int passed_second =  (now - start) ;
                            if (passed_second % 15 == 0){
                                char buffer[2048];
                                if ((int)(passed_second / 15) != 0 && (int)(passed_second / 15) != 1){
                                    if (state != 0){
                                    // send disconnect
                                        buffer[0] = 7;
                                        buffer[1] = 0;
                                        int fd_WR = open(path_pipe_WR, O_WRONLY);
                                        write(fd_WR, buffer, 2048);
                                        
                                    }
                                }
                                if ((int)(passed_second / 15) != 0){
                                    state = 1;
                                    buffer[0] = 5;
                                    buffer[1] = 0;
                                    int fd_RD = open(path_pipe_RD, O_WRONLY);
                                    write(fd_RD, buffer, 2048);
                        
                                    close(fd_RD);
                                    
                                }
                                sleep(1);
                            }else{
                                sleep(15 - passed_second % 15);
                            }
                        
                        

                        }
                    }else{
                        // Client Handler
                        while (1){
                            
                            char buffer_handler[2048];
                            // This open will block until client open WR pipe as O_WR;
                            int fd_handler_read = open(path_pipe_WR, O_RDONLY);
                            // Although this "read" is non-block, it will influenced by previous line
                            int nread = read(fd_handler_read, buffer_handler, sizeof(buffer_handler));
                            close(fd_handler_read);
        
                            if (nread == -1){
                                perror("Client Read error\n");
                                return 1;
                            }else if (nread == 0){
                                continue;
                            }else if (nread == 2048){

                                int type_0 = (int) buffer_handler[0];
                                int type_1 = (int) buffer_handler[1];
                                
                                if (type_0 == SAY && type_1 == TYPE_PADDING){
                                    char buffer_sent[2048];
                                    memset(buffer_sent, '\0', 2048);
                                    // Relay this to all other clients that are connected to same domain using the RECEIVE message.
                                    buffer_sent[0] = RECEIVE;
                                    buffer_sent[1] = TYPE_PADDING;
                                    copy_str_without_terminate(buffer_sent + 2, identifier, 256);
                                    copy_str_without_terminate(buffer_sent + 2 + 256, buffer_handler + 2, 1790);
                                    send_RECEIVE_to_other_client_handler_in_same_domain(buffer_sent, domain, identifier);
                                }
                                
                                else if (type_0 == RECEIVE && type_1 == TYPE_PADDING){
                                    int fd_RD = open(path_pipe_RD, O_WRONLY);
                                    write(fd_RD, buffer_handler, 2048);
                                    close(fd_RD);
                                }

                                // SAYCONT
                                // SAYCONT <message> <termination>
                                // type 2, message 1789, termination 1 (is reserved in 2048th -> buffer[2047])
                                else if (type_0 == SAYCONT && type_1 == TYPE_PADDING){
                                    char buffer_sent[2048];
                                    memset(buffer_sent, '\0', 2048);
                                    // relay this to all other clients that are connected to same domain using the RECCONT message.
                                    buffer_sent[0] = RECVCONT;
                                    buffer_sent[1] = TYPE_PADDING;
                                    copy_str_without_terminate(buffer_sent + 2, identifier, 256);
                                    copy_str_without_terminate(buffer_sent + 2 + 256, buffer_handler + 2, 1789);
                                    buffer_sent[2047] = buffer_handler[2047];
                                    send_RECEIVE_to_other_client_handler_in_same_domain(buffer_sent, domain, identifier);

                                }


                                // RECVCONT <identifier><message><termination>
                                // type 2(4), 256bytes   1789      1
                                else if (type_0 == RECVCONT && type_1 == TYPE_PADDING){
                                    int fd_RD = open(path_pipe_RD, O_WRONLY);
                                    write(fd_RD, buffer_handler, 2048);
                                    close(fd_RD);

                                }


                                // PONG client handler <-- client
                                // Send signal to PING handler which is the child of client handler 
                                else if (type_0 == PONG && type_1 == TYPE_PADDING){
                                    kill(pid_ping,SIGUSR2); 
                                }

                                else if (type_0 == DISCONNCT && type_1 == TYPE_PADDING){
                                    kill(pid_ping, SIGKILL);
                                    unlink(path_pipe_RD);
                                    unlink(path_pipe_WR);
                                    deleteFolder(domain);
                                    kill(getppid(), SIGUSR1);
                                    exit(0);
                                }  
                            }
                        }
                        
                    }

                }
               
            }

     

        }
        
    }

    
    return 0;
}
