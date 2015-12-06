typedef struct Node {
	int number; 
	bool status;
	char string[128];
	char extra_space[128];
	struct Node *next;
} Node;

void add(Node **h, Node **t, char *s, int n) {
	Node *new = calloc(1,sizeof(Node)); // setup
	new->number = n;
	strcpy(new->string,s);
	new->next = NULL;
	if (*h == NULL && *t == NULL) {
		*h = new;
		*t = new;
		//printf("creating. check me for lies. %d %d\n", (*h)->number, new->number);
		return;
	}
	//printf("adding %d %s, hopefully...\n", n, new->string);
	(*t)->next = new;
	*t = new;
}
/*void print_list(Node *h, bool s) {
	for (; h != NULL; h = h->next) {
		if (s) {
			char status[16] = "blank";
			if (h->status) {
				strcpy(status,"RUNNING");
			} else {
				strcpy(status,"PAUSED");
			}
			printf("%5d\t%s\t%s\n",h->number,h->string,status);
		} else {
			printf("%-5d%s\n",h->number,h->string);
		}
	}  
}
Node *remove_(Node **h, Node **t, Node *he, Node *ta, int n) { // must be freed after use
	//print_list(*h);
	if (he == NULL || ta == NULL) {
		return NULL;
	}
	if (he->number == n) {
		Node *pop = *h;
		*h = (*h)->next;
		return pop;
	}
	Node *prev = he;
	Node *curr = he->next;
	while (curr->number != n) {
		prev = prev->next;
		curr = curr->next;
		if (curr == ta) {
			return NULL;
		}
	}
	//curr = go_to(*h,curr->number);
	//prev = go_to(*h,prev->number);
	prev->next = curr->next;
	return curr;
}
Node *go_to(Node *h, int loc) {
	for (; h; h = h->next) { 
		if (h->number == loc) {
			return h;
		}
	} return NULL;
}*/

