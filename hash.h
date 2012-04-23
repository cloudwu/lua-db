#ifndef LUADB_HASH_H
#define LUADB_HASH_H

#include <stddef.h>
#include <lua.h>

#define TYPE_NIL 0
#define TYPE_NUMBER 1
#define TYPE_STRING 2
#define TYPE_BOOLEAN 3
#define TYPE_FUNCTION 4
#define TYPE_TABLE 5


struct table {
	int size;
	const char ** array;
};

struct bytes {
	const char * str;
	size_t len;
};

struct data {
	int type;
	union {
		struct bytes str;
		lua_Number n;
		int boolean;
		struct table * tbl;
	} v;
};

struct hash;

void hash_push(struct hash * self , const char * key , size_t sz, struct data * value);
struct data * hash_search(struct hash * self, const char * key , size_t sz);
size_t hash_genpool(struct hash * self);
struct hash * hash_new();
void hash_delete(struct hash *self);
void debug_table(struct hash *h);

#endif
