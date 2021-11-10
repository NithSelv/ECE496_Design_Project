#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_NAME 32
#define MAX_DATA 4053
struct Message {
	unsigned int type;
	unsigned int size;
	char source[MAX_NAME];
	char data[MAX_DATA];
};

static char* messageToBuffer(const struct Message *target_buffer) {
    
	
    printf("type is %d\n",target_buffer -> type); 
    printf("source is %s\n",target_buffer -> source); 
    printf("size is %d\n",target_buffer -> size); 
    printf("data is %s\n",target_buffer -> data); 	
    static char return_buffer[4096] = "";
    char temp[100];
    memset((void *)temp, 0, sizeof(temp));
    sprintf(temp, "%d", target_buffer->type);
    strcpy(return_buffer, temp);
    strcat(return_buffer, ";");
    memset((void *)temp, 0, sizeof(temp));
    sprintf(temp, "%d", target_buffer->size);
    strcat(return_buffer, temp);
    strcat(return_buffer, ";");
    strcat(return_buffer, target_buffer->source);
    strcat(return_buffer, ";");
    strcat(return_buffer, target_buffer->data);
    printf("current buffer is %s\n",return_buffer);
    
    
    return return_buffer;
    }

static struct Message* bufferToMessage(char* target_buffer) {
	static struct Message return_message;
	//Clean
	return_message.type = 999;
	return_message.size = 0;
	strcpy(return_message.data, "");
	strcpy(return_message.source, "");
	char * pch;
	pch = strtok (target_buffer,";");
	return_message.type = atoi(pch);
	pch = strtok (NULL,";");
	return_message.size = atoi(pch);
	pch = strtok (NULL,";");
        if(pch != NULL) {	
	    strcpy (return_message.source, pch);
	}
	pch = strtok (NULL,";");
	if(pch!= NULL){
	    strcpy(return_message.data, pch);
	}
	printf("type is %d\n",return_message.type); 
        printf("source is %s\n",return_message.source); 
        printf("size is %d\n",return_message.size); 
        printf("data is %s\n",return_message.data); 	
	


	return &return_message;
    }	
