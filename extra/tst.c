#include "../mem.c"
#include "../util.c"

int printarray(char *a[]) {
	int i;

	for (i = 0; a[i] != NULL; i++) {
		printf("Zeile %d: %s\n", i, a[i]);
	}

}

int main(void) {
	char replyheaderbuf[256];
	memdescr replyheadermempool = { sizeof(replyheaderbuf), 0, replyheaderbuf };
	char *replyheader[11] = { NULL };
	indexdescr replyheaderindex = { 10, 0, replyheader, &replyheadermempool };

	int i, rc;
	unsigned int n;

	for (n = 1024, i = 1; i < 14; n = n + n, i++) {
		printf("Durchgang %d: n=%d\n", i, n);
		rc = id_add_string(&replyheaderindex, "Aktueller Wert: ");
		printf("Add_string rc=%d\n", rc);
		if (rc == 0) {
			rc = md_extend_number(&replyheadermempool, n);
			printf("\n");
			printf("Extend_number rc=%d\n", rc);
		}
		printarray(replyheader);
	}

	return 0;
}
