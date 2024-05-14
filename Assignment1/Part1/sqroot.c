#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<math.h>
#include<unistd.h>
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Unable to execute\n");
        return 1;
    }

    int n = strtoul(argv[argc-1], NULL, 10);
    char c[32];
    sprintf(c,"%d",(int)floor( sqrt(n)));
    char *new_arr[argc];
    for (int i = 1; i < argc; i++) {
	if (i == 1) {
		char *temp = "./";
		new_arr[i-1] = malloc(strlen(temp) + strlen(argv[i]) + 1);
		strcpy(new_arr[i-1], temp);
		strcat(new_arr[i-1], argv[i]);
	} else if(i==argc-1){	
		new_arr[i-1] = c;
	}
	else new_arr[i-1]=argv[i];
    }
    new_arr[argc-1]=NULL;
    if(argc==2){
	if(strcmp(argv[0],"./sqroot")==0) printf("%d\n",(int)floor( sqrt(n)));
    }
    else if(argc<2) return 1;
    else {
	execvp(new_arr[0],new_arr);
    }
    return 0;
}
