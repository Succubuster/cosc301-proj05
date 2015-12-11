#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"

/*void print_indent(int indent)
{
    int i;
    for (i = 0; i < indent*4; i++)
	printf(" ");
}*/

/*typedef struct Node {
	////struct dirent *file;
	// needed for 1: name, size, numofclusters
    char name[9];
    char ext[4];
    uint32_t size;
    uint16_t sc;
    int numOfClusters;
    uint16_t *allChain;
	struct Node *next;
} dNode;


//uint16_t allocated[100];
//int numAllocated = 0;

void add(dNode **h, dNode **t, char *n, char *e, uint32_t s, uint16_t sc, int num, uint16_t ** arr) {
	dNode *new = calloc(1,sizeof(dNode)); // setup
	new->size = s;
	strcpy(new->name,n);
	strcpy(new->ext,e);
	new->sc = sc;
	new->numOfClusters = num;
	new->allChain = (uint16_t *) (*arr);
	new->next = NULL;
	if (*h == NULL && *t == NULL) {
		*h = new;
		*t = new;
		//printf("creating. check me for lies. %d %d\n", (*h)->number, new->number); // old debugging
		return;
	}
	//printf("adding %d %s, hopefully...\n", n, new->string); // old debugging
	(*t)->next = new;
	*t = new;
	return;
}
dNode *go_to(dNode *h, char *s) {
	for (; h; h = h->next) { 
		//printf("\t\t\t%s %s\n",h->name,s);
		if (strcmp(h->name, s) == 0) {
			return h;
		}
	} return NULL;
}

int in(uint16_t target, uint16_t *arr, int arrlen) {
	for (int i = 0; i < arrlen; i++) {
		if (arr[i] == target) {
			return 0;
		}
	} return -1;
}

int searchAllChains(dNode *h, uint16_t target) {
	for (;h;h=h->next) {
		if (in(target,h->allChain,h->numOfClusters) == 0) {
			return 0;
		}
	} return -1;
}

int getCN(uint16_t s, uint16_t** arr, uint8_t *image_buf, struct bpb33* bpb) {
	int i = 0;
	uint16_t cluster = s;
	//while (!is_end_of_file(cluster)) // not sure which is better
	while (is_valid_cluster(cluster, bpb)) 
    {
    	i++;
    	//allocated[numAllocated++] = cluster;
    	printf("%d\n",cluster);
		cluster = get_fat_entry(cluster, image_buf, bpb);
    } 
    printf("\n");
   	uint16_t *temp_arr = malloc(sizeof(uint16_t)*i);
   	cluster = s;
   	for (int x = 0; x < i; x++) {
   		temp_arr[x] = cluster;
   		cluster = get_fat_entry(cluster,image_buf,bpb);
   	}
   	*arr = temp_arr;
    return i;
}*/

uint16_t check_dirent(struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, int *clust_map)
{
    uint16_t followclust = 0;

    int i;
    char name[9];
    char extension[4];
    uint32_t size;
    uint16_t file_cluster;
    
    name[8] = ' ';
    extension[3] = ' ';
    memcpy(name, &(dirent->deName[0]), 8);
    memcpy(extension, dirent->deExtension, 3);
    if (name[0] == SLOT_EMPTY)
    {
	return followclust;
    }

    /* skip over deleted entries */
    if (((uint8_t)name[0]) == SLOT_DELETED)
    {
	return followclust;
    }

    if (((uint8_t)name[0]) == 0x2E)
    {
	// dot entry ("." or "..")
	// skip it
        return followclust;
    }
	
	// not sure if needed
    /* names are space padded - remove the spaces */
    for (i = 8; i > 0; i--) 
    {
	if (name[i] == ' ') 
	    name[i] = '\0';
	else 
	    break;
    }

    /* remove the spaces from extensions */
    for (i = 3; i > 0; i--) 
    {
	if (extension[i] == ' ') 
	    extension[i] = '\0';
	else 
	    break;
    }

    if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) 
    {
        if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN)
        {
            // when done, give next dest 
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    }
    else 
    {
     	size = getulong(dirent->deFileSize);
	
		/*dNode *t;
		if ((t = go_to(head,name)) != NULL) { // fix go_to URGENT
			printf("\tfound %s\n", t->name);
			//t->numOfClusters++;
		} else {
			uint16_t *arr;
			int num = getCN(getushort(dirent->deStartCluster), &arr, image_buf, bpb);
			add(&head,&tail,name,extension,size,getushort(dirent->deStartCluster),num,&arr);
		
		}*/
		
		int numOfClusters = 0;
		uint16_t cluster = getushort(dirent->deStartCluster);
		while (is_valid_cluster(cluster, bpb)) 
		{
			int clust_state = clust_map[cluster]++;
			numOfClusters++;
			if (clust_state > 1) {
				printf("Weird behavior at %d\n", cluster);
			}
			
			//printf("%d\n",cluster);
			cluster = get_fat_entry(cluster, image_buf, bpb);
		}
		
		int n = numOfClusters*512;
		printf("%s.%s (%u b) ? (%d sc) ? (%d l)\n", name, extension, size, getushort(dirent->deStartCluster), n);
		printf("\t");
		if (size == n) {
			printf("Fine.\n");
		} else if (size > n) {
			int diff = ((int)size - n) / 512;
			if (((int)size - n) % 512 != 0) {
				diff++;
			}
			//printf("Needs %d more allocated\n", diff);
			
			
			printf("%d unrecoverable, metadata shortened from %d to %d",size-n,dirent->deFileSize,(diff-1)*512);
			putulong(dirent->deFileSize,(diff - 1) * 512);
		} else if (size > n - 512) {
			printf("Fine, I guess...\n");
		} else {
			printf("Has too many allocated by %d\n", (n - (int) size) / 512);
			cluster = getushort(dirent->deStartCluster);
			int i = 1;
			printf("\n");
			while (is_valid_cluster(cluster, bpb) && i != numOfClusters) {
				printf("%d, ", cluster);
				i++;
				cluster = get_fat_entry(cluster, image_buf, bpb);
			}
			printf("\n");
			uint16_t extra = get_fat_entry(cluster,image_buf,bpb);
			clust_map[extra]--;
			set_fat_entry(extra,CLUST_FREE&FAT12_MASK,image_buf,bpb);
			set_fat_entry(cluster, CLUST_EOFS&FAT12_MASK,image_buf,bpb);
			
				
		}
	} return followclust;
}

void follow_dir(uint16_t cluster, uint8_t *image_buf, struct bpb33* bpb, int *clust_map)
{
    while (is_valid_cluster(cluster, bpb))
    {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry);
        int i = 0;
	for ( ; i < numDirEntries; i++)
	{
            // action
            uint16_t followclust = check_dirent(dirent, image_buf, bpb, clust_map);
            // print_dirent(dirent, indent);
            if (followclust) {
            	clust_map[followclust]++;
                follow_dir(followclust, image_buf, bpb, clust_map);
            } dirent++;
	}
	// this is important
	cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}

void traverse_root(uint8_t *image_buf, struct bpb33* bpb, int *clust_map)
{
    uint16_t cluster = 0;

    struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

    int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    {
        //  action
        uint16_t followclust = check_dirent(dirent, image_buf, bpb, clust_map);
        // print_dirent(dirent, 0);
        if (is_valid_cluster(followclust, bpb)) {
        	clust_map[followclust]++;
            follow_dir(followclust, image_buf, bpb, clust_map);
        }

        dirent++;
    }
}


//testing git pull CK
void usage(char *progname) {
    fprintf(stderr, "usage: %s <imagename>\n", progname);
    exit(1);
}


int main(int argc, char** argv) {
    uint8_t *image_buf;
    int fd;
    struct bpb33* bpb;
    if (argc < 2) {
	usage(argv[0]);
    }

    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);

    int *cluster_map = malloc(sizeof(int) * bpb->bpbSectors);
	for (int i = 0; i < bpb->bpbSectors; i++) {
		cluster_map[i] = 0; // no refs
	}
	
	printf("\n");
	traverse_root(image_buf,bpb,cluster_map);
	
	// orphan saving...
	printf("\n");
	for (int i = CLUST_FIRST; i < bpb->bpbSectors; i++) {
		uint16_t r = get_fat_entry(i,image_buf,bpb);
		if (cluster_map[i] == 0) {
			if (r == (CLUST_FREE & FAT12_MASK)) {
				//printf("Confused?: %d\n", i);
			} else if (r == (CLUST_BAD & FAT12_MASK)) {
				printf("Bad?: %d\n", i);
			} else {
				printf("Orphan?: %d\n", i);
			}
		}
	}
	
	
	
    
    
    
    
    
    // your code should start here...
	//traverse_root(image_buf, bpb);
	/*printf("\n");
	for (dNode *h=head; h != NULL; h=h->next) {
		uint32_t s = h->size;
		int n = h->numOfClusters*512;
		printf("\t%s.%s (%u b) ? (%d sc) ? (%d l)\n", h->name, h->ext, s, h->sc, n);
		if (s % 512 == 0) {
			printf("%s\n", s == n ? "Fine" : "Not Fine");
		}
		printf("\t\t");
		if (s == n) {
			printf("Fine.\n");
		} else if (s > n) {
			int diff = ((int)s - n) / 512;
			if (((int)s - n) % 512 != 0) {
				diff++;
			}
			printf("Needs %d more\n", diff);
		} else if (s > n - 512) {
			printf("Fine, I guess...\n");
		} else {
			printf("Has too many allocated by %d\n", (n - (int) s) / 512);
		}
	}
	printf("\n");*/
	
	/*uint8_t *base = root_dir_addr(image_buf, bpb) + getushort(bpb->bpbRootDirEnts) * sizeof(struct direntry);
	uint8_t *p; 
	uint16_t bs = getushort(bpb->bpbBytesPerSec) * bpb->bpbSecPerClust;
	int free_count = 0;*/
    //p = root_dir_addr(image_buf, bpb);
    /*if (cluster != MSDOSFSROOT) {

		base += ;
	}*/
	
	/*for (int n = (CLUST_FIRST & FAT12_MASK); n < (CLUST_LAST & FAT12_MASK); n++) {
		p = base + bs * n; // pointer hopping
		uint8_t val = *p;
		if ((val & CLUST_FREE) == 1) {
			free_count++;
		}
    }
    printf("\tFrees: %d\n", free_count);*/
    //printf("\t\t");
    //uint16_t arrlen = (bpb->bpbSectors / bpb->bpbSecPerClust) & FAT12_MASK;
    //uint16_t ff[arrlen-2];
   //for (uint16_t num = CLUST_FIRST; is_valid_cluster(num,bpb) /*&& num < 905  print in intervals of 1000 or use sleep below*/; num++) {
    	/*uint16_t res = get_fat_entry(num,image_buf,bpb) & FAT12_MASK;
    	//printf("\tResult from FAT: %d\n", res);
    	//ff[(num-CLUST_FIRST)] = res;
    	printf("%d -> ", num);
    	if (res == (CLUST_FREE & FAT12_MASK)) {
    		printf("free\n");
    	} else if (res == (CLUST_BAD & FAT12_MASK)) {
    		printf("bad\n");
    	} else if (is_end_of_file(res)) {
    		printf("eof\n");
    	} else {
    		printf("%d\n",res);
    		if (searchAllChains(head,num) == -1) {
    			printf("\t\tsomething happened.\n");
    		} else {
    			//printf("\t\tSomething else happened.\n");
    		}
    	} usleep(200000);
    }*/
    //printf("\n");
    //dNode *head = NULL; // maybe obsol?
	//dNode *tail = NULL; //   ""
	
  	unmmap_file(image_buf, &fd);
    return 0;
}
