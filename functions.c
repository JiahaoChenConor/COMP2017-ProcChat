#include "server.h"

// type = 0, it is RD, type == 1, it is WR
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

int deleteFolder(char* domain){
    int file_count = 0;
    DIR *dir;
    struct dirent *entry;
    dir = opendir(domain);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_FIFO)
            file_count++;
    }
    closedir(dir);
    if (file_count == 0)
        remove(domain);
    return 0;

}

int creatFolder(char *folder_name){
    // folder already exists
    if (access(folder_name, W_OK) == 0){
        return 0;
    }

    int ret = mkdir(folder_name, S_IRWXU);
    if (ret == 0){
        return 0;
    }else{
    
        return 1;
    }
}



void str_copy_extra_termination(char* destination, char* resource, int len){
    for (int i = 0; i < len - 1; i++){
        destination[i] = resource[i];
    }

    destination[len - 1] = '\0'; 
}


void copy_str_without_terminate(char* destination, char* resource, int len){
    for (int i = 0; i < len; i++){
        destination[i] = resource[i];
    }
}



void send_RECEIVE_to_other_client_handler_in_same_domain(char* buffer_sent, char* domain, char* sender){
    DIR *dirp;
    struct dirent *dp;
    dirp = opendir(domain);
    while ((dp = readdir(dirp)) != NULL)
    {
        char *name_of_pipes = dp->d_name;
        
        // If the name of this pipe cotains _RD and it is not the current path_pipe_RD.
        // Then write buffer_sent into this _RD pipe
        // Achieve the RECEIVE
        int is_different_pipe = 0;
        
        int offset = 0;
        // If any char is different, they are differnt pipes
        while (*(sender + offset) != '\0'){
            if (*(sender + offset) != *(name_of_pipes + offset)){
                is_different_pipe = 1;
                break;
            }
            offset++;
        }
        
        int is_WR_pipe = 0;
        int i = 0;
        for (i = 0; name_of_pipes[i] != '\0'; i++){}
        // impossible to out of range, since every pipe include _RD/_WR
        if (i >= 3){
            if (name_of_pipes[i - 3] == '_' && name_of_pipes[i- 2] == 'R' 
            && name_of_pipes[i - 1] == 'D' && name_of_pipes[i] == '\0')
            {
                is_WR_pipe = 1;
            }
        }
        

        if (is_different_pipe && is_WR_pipe){
            char path_other_WR[517];
            set_path(domain, NULL, name_of_pipes, path_other_WR, 1);
            int fp_RD = open(path_other_WR, O_WRONLY);
            write(fp_RD, buffer_sent, 2048);
            close(fp_RD);
        }
    }
    
    (void) closedir(dirp);

}
