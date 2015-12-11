#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"

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
    if (name[0] == SLOT_EMPTY || ((uint8_t)name[0]) == SLOT_DELETED || ((uint8_t)name[0]) == 0x2E) {
		return followclust;
    }
	
	// not sure if needed
    /* names are space padded - remove the spaces */
    for (i = 8; i > 0; i--) {
		if (name[i] == ' ') 
			name[i] = '\0';
		else 
			break;
    }

    /* remove the spaces from extensions */
    for (i = 3; i > 0; i--) {
		if (extension[i] == ' ') 
			extension[i] = '\0';
		else 
			break;
    }

    if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) {
        if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN) {
            // when done, give next dest 
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    } else {
     	size = getulong(dirent->deFileSize);
	
		int numOfClusters = 0;
		uint16_t cluster = getushort(dirent->deStartCluster);
		uint16_t prev;
		while (is_valid_cluster(cluster, bpb)) {
			int clust_state = clust_map[cluster]++;
			if (clust_state > 1) { // multiple refs?
				dirent->deName[0] = SLOT_DELETED;
				clust_map[cluster]--;
				printf("\nWell, isn't %d popular?\n", cluster); 
			}
			prev = cluster;
			cluster = get_fat_entry(cluster, image_buf, bpb);
			if (prev == cluster) { // points to self in FAT
				printf("\nWas %d just cycling?\n", cluster);
				set_fat_entry(cluster, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
				numOfClusters++;
				break;
			}
			if (cluster == (FAT12_MASK & CLUST_BAD)) {
				printf("\nI think %d has been bad, someone getting coal.\n", prev);
				set_fat_entry(cluster, FAT12_MASK & CLUST_FREE, image_buf, bpb);
				set_fat_entry(prev, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
				numOfClusters++;
				break;  
			} numOfClusters++;
		}
		printf("%s.%s\n", name, extension);
		
		int n = numOfClusters*512;
		printf("\t");
		if (size == n) {
			printf("Fine.\n");
		} else if (size > n) {
			int diff = ((int)size - n) / 512;
			if (((int)size - n) % 512 != 0) {
				diff++;
			}
			printf("%d bytes unrecoverable, metadata shortened from %u to %d\n",size-n,size,n);
			putulong(dirent->deFileSize,n);
		} else if (size > n - 512) {
			printf("Fine, I guess...\n");
		} else {
			int i = (n - (int) size) / 512;
			printf("Has too many allocated by %d\n", i);
			i++; // Needed to hold prev chunk
			cluster = getushort(dirent->deStartCluster);
			//printf("\n");
			while (is_valid_cluster(cluster, bpb) && i != numOfClusters) {
				i++;
				cluster = get_fat_entry(cluster, image_buf, bpb);
			}
			//printf("\n\n");
			uint16_t extra = get_fat_entry(cluster,image_buf,bpb);
			set_fat_entry(cluster, CLUST_EOFS&FAT12_MASK,image_buf,bpb);
			while (1) {
				clust_map[extra]--;
				numOfClusters--;
				if (!is_valid_cluster(extra,bpb)) {
					break;
				}
				printf("\tRemoving cluster %u.\n", extra);
				uint16_t next = get_fat_entry(extra,image_buf,bpb);
				set_fat_entry(extra,CLUST_FREE&FAT12_MASK,image_buf,bpb);
				extra = next;
			}
		}
	} return followclust;
}

void follow_dir(uint16_t cluster, uint8_t *image_buf, struct bpb33* bpb, int *clust_map) {
    while (is_valid_cluster(cluster, bpb)) {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry);
        int i = 0;
		for ( ; i < numDirEntries; i++) {
            // action
            uint16_t followclust = check_dirent(dirent, image_buf, bpb, clust_map);
            if (followclust) {
            	clust_map[followclust]++;
                follow_dir(followclust, image_buf, bpb, clust_map);
            } dirent++;
		} cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}

void traverse_root(uint8_t *image_buf, struct bpb33* bpb, int *clust_map) {
    uint16_t cluster = 0;
	struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);
	int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    {
        //  action
        uint16_t followclust = check_dirent(dirent, image_buf, bpb, clust_map);
        if (is_valid_cluster(followclust, bpb)) {
        	clust_map[followclust]++;
            follow_dir(followclust, image_buf, bpb, clust_map);
        } dirent++;
    }
}

void write_dirent(struct direntry *dirent, char *filename, 
		  uint16_t start_cluster, uint32_t size)
{
    char *p, *p2;
    char *uppername;
    int len, i;

    /* clean out anything old that used to be here */
    memset(dirent, 0, sizeof(struct direntry));

    /* extract just the filename part */
    uppername = strdup(filename);
    p2 = uppername;
    for (i = 0; i < strlen(filename); i++) 
    {
	if (p2[i] == '/' || p2[i] == '\\') 
	{
	    uppername = p2+i+1;
	}
    }

    /* convert filename to upper case */
    for (i = 0; i < strlen(uppername); i++) 
    {
	uppername[i] = toupper(uppername[i]);
    }

    /* set the file name and extension */
    memset(dirent->deName, ' ', 8);
    p = strchr(uppername, '.');
    memcpy(dirent->deExtension, "___", 3);
    if (p == NULL) 
    {
	fprintf(stderr, "No filename extension given - defaulting to .___\n");
    }
    else 
    {
	*p = '\0';
	p++;
	len = strlen(p);
	if (len > 3) len = 3;
	memcpy(dirent->deExtension, p, len);
    }

    if (strlen(uppername)>8) 
    {
	uppername[8]='\0';
    }
    memcpy(dirent->deName, uppername, strlen(uppername));
    free(p2);

    /* set the attributes and file size */
    dirent->deAttributes = ATTR_NORMAL;
    putushort(dirent->deStartCluster, start_cluster);
    putulong(dirent->deFileSize, size);

    /* could also set time and date here if we really
       cared... */
}

void create_dirent(struct direntry *dirent, char *filename, 
		   uint16_t start_cluster, uint32_t size,
		   uint8_t *image_buf, struct bpb33* bpb)
{
    while (1) {
		if (dirent->deName[0] == SLOT_EMPTY) {
			/* we found an empty slot at the end of the directory */
			write_dirent(dirent, filename, start_cluster, size);
			dirent++;

			/* make sure the next dirent is set to be empty, just in
			   case it wasn't before */
			memset((uint8_t*)dirent, 0, sizeof(struct direntry));
			dirent->deName[0] = SLOT_EMPTY;
			return;
		}

		if (dirent->deName[0] == SLOT_DELETED) 
		{
			/* we found a deleted entry - we can just overwrite it */
			write_dirent(dirent, filename, start_cluster, size);
			return;
		} dirent++;
    }
}

void orphan_handler(uint8_t *image_buf, struct bpb33* bpb, int *cluster_map) {
	int orphans = 0;
	int *orphan_map = malloc(sizeof(int) * bpb->bpbSectors);
	for (int i = 0; i < bpb->bpbSectors; i++) {
		orphan_map[i] = 0; // no refs
	}
	for (int i = CLUST_FIRST; i < bpb->bpbSectors; i++) {
		uint16_t r = get_fat_entry(i,image_buf,bpb);
		if (cluster_map[i] == 0 && r != (CLUST_FREE & FAT12_MASK)) {
				//printf("Confused?: %d\n", i);
			if (r == (CLUST_BAD & FAT12_MASK)) {
				printf("Bad?: %d\n", i);
			} 
			if (r != (CLUST_BAD & FAT12_MASK) && orphan_map[i] == 0) {
				//printf("Orphan?: %d\n", i);
				orphans++;
				char num[5];
				sprintf(num, "%d", orphans);
				char fn[64] = "";
				strcat(fn, "found");
				strcat(fn, num); 
				strcat(fn, ".dat");
				//char *file = fn; 
				printf("Taking %d to lost and found.\nWon't tell me his name, so I'm calling them %s.\nPoor kid\n", i, fn);
				uint16_t cluster = i;
				int s = 0;
				while (is_valid_cluster(cluster, bpb)) {
					s++;
					orphan_map[cluster]++;
					cluster = get_fat_entry(cluster, image_buf, bpb);
				}
				create_dirent((struct direntry*)root_dir_addr(image_buf, bpb), fn, i, 512*s, image_buf, bpb); 
			}
		} 
	} free(orphan_map);
}

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
	
	printf("Before:\n");
	traverse_root(image_buf,bpb,cluster_map);
	printf("\n");
	orphan_handler(image_buf, bpb, cluster_map);
	
	printf("\n");
	printf("After:\n");
	traverse_root(image_buf,bpb,cluster_map);
	printf("\n");
	orphan_handler(image_buf, bpb, cluster_map);
	
	free(cluster_map);
  	unmmap_file(image_buf, &fd);
    return 0;
}
