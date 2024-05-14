#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include<math.h> 
#define MIN_CHUNK_SIZE (4 * 1024 * 1024)
unsigned long *head = NULL;

void *memalloc(unsigned long size) 
{
	if(size==0) return NULL;
	size += ((8 - size % 8)%8);
	unsigned long reqsize= (size+8) >24 ? size+8 : 24;
	if(head == NULL){
		unsigned long chunkSize = MIN_CHUNK_SIZE*((MIN_CHUNK_SIZE+reqsize-1)/MIN_CHUNK_SIZE);
		unsigned long * ptr = mmap(NULL, chunkSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(ptr == NULL)return ptr;
		ptr[0] = reqsize;
		head = ptr + (reqsize) /8;
		head[0] = chunkSize - reqsize;
		head[1] = NULL;
		head[2] = NULL;
		return ptr + 1; 
	}

	unsigned long * curr=head;
	while(curr!=NULL){
		if(curr[0]>=reqsize){
			if(curr==head){
				unsigned long b=curr[0]-reqsize;
				if(b>=24){
					unsigned long *ptr=curr;
					ptr[0]=reqsize;
					head=ptr+(reqsize)/8;
					head[0]=b;
					head[1]=NULL;
					head[2]=NULL;
					return ptr+1;
				}
				head=NULL;
				return curr+1;
			}
			unsigned long b=curr[0]-reqsize;
			if(b>=24){
				unsigned long *ptr=curr+(reqsize)/8;
				unsigned long * first = head;
				ptr[0]= b;
				ptr[1]=first;
				ptr[2] = NULL;
				if(first != NULL){
					first[2] = ptr;
				}
				head=ptr;
				unsigned long * prev=curr[2], *next= curr[1];
				if(prev != NULL)
					prev[1]=next;
				if(next != NULL)
					next[2]=prev;
				curr[0] = reqsize;
				return curr+1;
			}
			unsigned long * prev=curr[2], *next= curr[1];
			if(prev != NULL)
					prev[1]=next;
			if(next != NULL)
					next[2]=prev;
			return curr+1;

		}
		unsigned long * next=curr[1];
		curr=next;
	}
	unsigned long chunkSize = MIN_CHUNK_SIZE*((MIN_CHUNK_SIZE+reqsize-1)/MIN_CHUNK_SIZE);
	unsigned long * ptr = mmap(NULL, chunkSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(ptr == NULL)return ptr;
	ptr[0]=reqsize;
	unsigned long *temp=ptr+reqsize/8;
	temp[0]=chunkSize-reqsize;
	unsigned long * first = head;
	temp[1]=first;
	if(first != NULL){
		first[2] = temp;
	}
	temp[2]=NULL;
	head=temp;
	return ptr+1;
}

int memfree(void *ptr)
{
	if(ptr==NULL) return -1;
	unsigned long size=*((unsigned long *)ptr-1);
	unsigned long * curr=(unsigned long*)ptr-1;
	unsigned long *next=NULL,*prev=NULL;
	unsigned long * temp=head;
	unsigned long * pt=ptr-1;
	while(temp!=NULL){
		if(pt+pt[0]/8==temp) {next=temp;break;}
		unsigned long *t=temp[1];
		temp=t;
	}
	temp=head;
	while(temp!=NULL){
		if(temp+temp[0]/8==pt-1) {prev=temp;break;}
		unsigned long *t=temp[1];
		temp=t;
	}
	
	if(!prev && !next){
		
		unsigned long *first=head;
		curr[0]=size;
		curr[1]=first;
		if(first!=NULL){
			first[2]=curr;
			}
		curr[2]=NULL;
		head=curr;
		return 0;
	}
	if(!prev && next){
		curr[0]+=next[0];
		if(next==head){
			
			if(next[1]!=NULL) curr[1]=next[1];
			curr[2]=NULL;
			head=curr;
			return 0;
		}
		unsigned long* prv= next[2], *nxt=next[1];
		curr[1]=NULL;curr[2]=NULL;
		if(prv != NULL)
				prv[1]=nxt;
		if(nxt != NULL)
				nxt[2]=prv;
		unsigned long * first=head;
		curr[1]=first;
		if(first!=NULL) first[2]=curr;
		//curr[2]=NULL;
		head=curr;
		return 0;
	}
	if(prev && !next){
		unsigned long *first=head;
		curr[0]+=prev[0];
		if(prev==head){
			prev[0]+=curr[0];
			return 0;
		}
		prev[0]+=curr[0];
		unsigned long *nxt=prev[1],*prv=prev[2];
		if(prv != NULL)
				prv[1]=nxt;
		if(nxt != NULL)
				nxt[2]=prv;
		prev[1]=first;
		if(first!=NULL) first[2]=prev;
		prev[2]=NULL;
		head=prev;
		return 0;
	}
	
	if(prev && next){
		unsigned long *first=head;
		prev[0]+=curr[0]+next[0];
		if(prev==head){
			if(prev[1]!=next) return -1;
			prev[1]=next[1];
			return 0;
		}
		unsigned long *prv=prev[2],*nxt=next[1];
		if(prv != NULL)
				prv[1]=nxt;
		if(nxt != NULL)
				nxt[2]=prv;
		prev[1]=first;
		if(first!=NULL) first[2]=prev;
		prev[2]=NULL;
		head=prev;
		return 0;
		
	}
	return 0;
}	
