#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
 
unsigned long calculateDirectorySize(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        return 0;
    }
    struct stat st_curr;
    if(stat(path,&st_curr)==-1) {
    	perror("Error getting file status");
            
    }
    unsigned long totalSize = st_curr.st_size;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
 
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
 
        struct stat st;
        if (stat(fullPath, &st) == -1) {
            perror("Error getting file status");
            continue;
        }
 	if (S_ISLNK(st.st_mode)) {
            char link_target[PATH_MAX];
            ssize_t link_len = readlink(fullPath, link_target, sizeof(link_target) - 1);
 
            if (link_len == -1) {
                perror("readlink");
                continue;
            }
 
            link_target[link_len] = '\0';
            totalSize +=calculateDirectorySize(link_target);
            
        } 
        else if (S_ISDIR(st.st_mode)) {
            totalSize += calculateDirectorySize(fullPath);
        } else {
            totalSize += st.st_size;
        }
    }
 
    closedir(dir);
 
    return totalSize;
}


int main(int argc,char *argv[]){
const char *startPath = "."; // Default to current working directory
 
    if (argc > 1) {
        startPath = argv[1];
    }
 
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Error creating pipe");
        return 1;
    }
 
    DIR *dir= opendir(startPath);
    if (dir == NULL) {
        perror("Error opening directory");
        return 0;
    }
    
    struct stat st_curr;
    if(stat(startPath,&st_curr)==-1) {
    	perror("Error getting file status");
            
    }
    
    unsigned long totalSize = st_curr.st_size;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
 
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", startPath, entry->d_name);
 
        struct stat st;
        if (stat(fullPath, &st) == -1) {
            perror("Error getting file status");
            continue;
        }
 	if (S_ISLNK(st.st_mode)) {
            char link_target[PATH_MAX];
            ssize_t link_len = readlink(fullPath, link_target, sizeof(link_target) - 1);
 
            if (link_len == -1) {
                perror("readlink");
                continue;
            }
 
            link_target[link_len] = '\0';
            //printf("Symbolic link: %s -> %s\n", path, link_target);
            totalSize +=calculateDirectorySize(link_target);
            
        } 
        else if (S_ISDIR(st.st_mode)) {
            // Recursively calculate sub-directory size
            //totalSize += st.st_size+calculateDirectorySize(fullPath);
            int pid;
            if((pid=fork())<0){
            	perror("fork");
            	exit(-1);
            	}
            if(!pid){
            	unsigned long subsize=calculateDirectorySize(fullPath);
            	write(pipe_fd[1], &subsize, sizeof(subsize));
       		exit(0);
       		}
       	    unsigned long buf;
       	    read(pipe_fd[0],&buf,sizeof(buf));
       	    totalSize+=buf;
        } else {
            // Add file size to total
            totalSize += st.st_size;
        }
    }
    
    
 	printf("%lu\n", totalSize);
    return 0;
}
