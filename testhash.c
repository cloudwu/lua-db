#include "hash.h"
#include <string.h>
#include <stdio.h>

static void
push_number(struct hash * self, const char *key, lua_Number n) {
	struct data d;
	d.type = TYPE_NUMBER;
	d.v.n = n;
	hash_push(self, key, strlen(key), &d);
}

static void
push_string(struct hash * self, const char *key, const char *value) {
	struct data d;
	d.type = TYPE_STRING;
	d.v.str.str = value;
	d.v.str.len = strlen(value);
	hash_push(self, key, strlen(key), &d);
}

static void
push_nil(struct hash * self, const char *key) {
	struct data d;
	d.type = TYPE_NIL;
	hash_push(self, key, strlen(key), &d);
}

static void
read_hash(struct hash *h, const char *key) {
	struct data * d = hash_search(h, key, strlen(key));
	if (d== NULL) {
		printf("%s = (null)\n",key);
	} else {
		printf("%s = ",key);
		switch(d->type) {
		case TYPE_NIL:
			printf("nil");
			break;
		case TYPE_NUMBER:
			printf("%lf",d->v.n);
			break;
		case TYPE_FUNCTION:
			printf("(function)");
			break;
		case TYPE_STRING:
			printf("%s",d->v.str.str);
			break;
		case TYPE_TABLE:
			printf("[table]");
			break;
		}
		printf("\n");
	}
}

static void
test(struct hash *h) {
	push_number(h, "a" , 1.0);
	push_string(h, "b" , "hello world");
	push_nil(h, "c");
	int i;
	char buffer[10];
	for (i=0;i<100;i++) {
		sprintf(buffer,"%d",i*2);
		push_number(h,buffer,i);
	}
	debug_table(h);
	hash_genpool(h);
}

static void
search(struct hash *h) {
	read_hash(h,"a");
	read_hash(h,"b");
	read_hash(h,"c");
	int i;
	char buffer[10];
	for (i=0;i<100;i++) {
		sprintf(buffer,"%d",i*2);
		read_hash(h,buffer);
	}
}

int
main() {
	struct hash * h = hash_new();
	test(h);
	search(h);
	hash_delete(h);
	return 0;
}
