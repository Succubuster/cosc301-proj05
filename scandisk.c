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

typedef struct Node {
	////struct dirent *file;
	// needed for 1: name, size, numofclusters
    char name[9];
    char ext[4];
    uint32_t size;
    uint16_t sc;
    int numOfClusters;
	struct Node *next;
} dNode;

dNode *head = NULL; // make global
dNode *tail = NULL; //   ""

void add(dNode **h, dNode **t, char *n, char *e, uint32_t s, uint16_t sc, int num) {
	dNode *new = calloc(1,sizeof(dNode)); // setup
	new->size = s;
	strcpy(new->name,n);
	strcpy(new->ext,e);
	new->sc = sc;
	new->numOfClusters = num;
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

int getCN(uint16_t s, uint8_t *image_buf, struct bpb33* bpb) {
	int i = 1;
	uint16_t cluster = s;
	while (is_valid_cluster(cluster, bpb))
    {
    	i++;
		cluster = get_fat_entry(cluster, image_buf, bpb);
    } return i;
}

uint16_t check_dirent(struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb)
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

    if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN)
    {
	// ignore any long file name extension entries
	//
	// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_VOLUME) != 0) 
    {
	//printf("Volume: %s\n", name);
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) 
    {
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
		if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN)
        {
	    	//print_indent(indent);
    	    //printf("%s/ (directory)\n", name);
            
            
            // when done, give next dest 
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    }
    else 
    {
        /*
         * a "regular" file entry
         * attributes, size, starting cluster, etc.
         */
	/*int ro = (dirent->deAttributes & ATTR_READONLY) == ATTR_READONLY;
	int hidden = (dirent->deAttributes & ATTR_HIDDEN) == ATTR_HIDDEN;
	int sys = (dirent->deAttributes & ATTR_SYSTEM) == ATTR_SYSTEM;
	int arch = (dirent->deAttributes & ATTR_ARCHIVE) == ATTR_ARCHIVE;*/

	size = getulong(dirent->deFileSize);
	
	dNode *t;
	if ((t = go_to(head,name)) != NULL) { // fix go_to URGENT
		printf("\tfound %s\n", t->name);
		//t->numOfClusters++;
	} else {
		int num = getCN(getushort(dirent->deStartCluster), image_buf, bpb);
		add(&head,&tail,name,extension,size,getushort(dirent->deStartCluster),num);
		
	}
	//print_indent(indent);
	/*printf("%s.%s (%u bytes) (starting cluster %d) %c%c%c%c\n", 
	       name, extension, size, getushort(dirent->deStartCluster),
	       ro?'r':' ', 
               hidden?'h':' ', 
               sys?'s':' ', 
               arch?'a':' ');*/
    }

    return followclust;
}





void follow_dir(uint16_t cluster, int level,
		uint8_t *image_buf, struct bpb33* bpb)
{
    while (is_valid_cluster(cluster, bpb))
    {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry);
        int i = 0;
	for ( ; i < numDirEntries; i++)
	{
            // action
            uint16_t followclust = check_dirent(dirent, image_buf, bpb);
            // print_dirent(dirent, indent);
            if (followclust)
                follow_dir(followclust, level+1, image_buf, bpb);
            dirent++;
	}
	// this is important
	cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}


void traverse_root(uint8_t *image_buf, struct bpb33* bpb)
{
    uint16_t cluster = 0;

    struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

    int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    {
        //  action
        uint16_t followclust = check_dirent(dirent, image_buf, bpb);
        // print_dirent(dirent, 0);
        if (is_valid_cluster(followclust, bpb))
            follow_dir(followclust, 1, image_buf, bpb);

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

    // your code should start here...
	traverse_root(image_buf, bpb);
	printf("\n");
	for (dNode *h=head; h != NULL; h=h->next) {
		uint32_t s = h->size;
		int n = h->numOfClusters*512 - 512;
		printf("\t%s.%s (%u b) ? (%d sc) ? (%d l)\n", h->name, h->ext, s, h->sc, n);
		/*if (s % 512 == 0) {
			printf("%s\n", s == n ? "Fine" : "Not Fine");
		} */
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
	printf("\n");




    unmmap_file(image_buf, &fd);
    return 0;
}
