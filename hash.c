#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <lua.h>
#include <lauxlib.h>
#include <string.h>

#include "hash.h"

#define STRING_INDEX 1

struct hash_node {
	uint32_t hash;
	const char * key;
	struct data value;	
	int next;
};

struct hash {
	int size;
	struct hash_node * node;
	int lastfree;
	char * stringpool;
	lua_State * temp;
};

static uint32_t
calc_hash(const char * str, size_t sz) {
	uint32_t h = (uint32_t)sz;
	size_t step = (sz>>5)+1;
	size_t l1;
	for (l1=sz; l1>=step; l1-=step) {
		h = h ^ ((h<<5)+(h>>2)+(uint32_t)str[l1-1]);
	}
	return h;
}

static const char *
store_string(lua_State *L, const char * str, size_t sz) {
	lua_pushlstring(L, str, sz);
	const char * n = lua_tostring(L, -1);
	lua_pushlightuserdata(L, (void *)n);
	lua_insert(L, -2);
	lua_rawset(L, STRING_INDEX);
	return n;
}

void
debug_table(struct hash *h) {
	lua_State *L = h->temp;
	lua_pushnil(L);
	while (lua_next(L, STRING_INDEX) != 0) {
		printf("%p - %s\n",
			lua_touserdata(L,-2),
			lua_tostring(L,-1));
		lua_pop(L, 1);
	}
}

static inline struct hash_node *
mainposition(struct hash * self, uint32_t hash) {
	return &self->node[(hash & (self->size - 1 ))];
}

static struct hash_node *
findfree(struct hash * self) {
	while (self->lastfree >= 0) {
		struct hash_node * n = &self->node[self->lastfree--];
		if (n->key == NULL) {
			return n;
		}
	}
	return NULL;
}

static struct table *
copy_table(lua_State *L, struct table * from) {
	struct table * to = malloc(sizeof(*from) + from->size * sizeof(const char *));
	to->size = from->size;
	to->array = (const char **)(to + 1);
	int i;
	for (i=0;i<to->size;i++) {
		to->array[i] = store_string(L, from->array[i], strlen(from->array[i]));
	}
	return to;
}

static void
push_value(struct hash * self, struct hash_node *n , struct data * value, bool copy) {
	if (copy) {
		memcpy(&n->value , value, sizeof(*value));
		return;
	}
	n->value.type = value->type;
	switch(value->type) {
	case TYPE_NIL:
		break;
	case TYPE_BOOLEAN:
		n->value.v.boolean = value->v.boolean;
		break;
	case TYPE_NUMBER:
		n->value.v.n = value->v.n;
		break;
	case TYPE_STRING:
	case TYPE_FUNCTION:
		n->value.v.str.str = store_string(self->temp, value->v.str.str, value->v.str.len);
		n->value.v.str.len = value->v.str.len;
		break;
	case TYPE_TABLE:
		n->value.v.tbl = copy_table(self->temp, value->v.tbl);
		break;
	}
}

static void expend_hash(struct hash * self);

static void
push_hashkey(struct hash * self , const char * str , uint32_t hash, struct data * value, bool copy) {
	struct hash_node *mn = mainposition(self,hash);
	if (mn->key == NULL) {
		mn->key = str;
		mn->hash = hash;
		push_value(self, mn , value , copy);
		return;
	}
	if (mainposition(self, mn->hash) != mn) {
		const char * tempk = mn->key;
		uint32_t temph = mn->hash;
		struct data tempv = mn->value;
		mn->key = str;
		mn->hash = hash;
		push_value(self, mn, value , copy);
		push_hashkey(self, tempk, temph, &tempv , true);
		return;
	}
	struct hash_node * free = findfree(self);
	if (free == NULL) {
		expend_hash(self);
		push_hashkey(self, str, hash, value , copy);
		return;
	}
	free->key = str;
	free->hash = hash;
	push_value(self, free, value, copy);
	free->next = mn->next;
	mn->next = free - self->node;
}

static void
expend_hash(struct hash * self) {
	int osize = self->size;
	struct hash_node * onode = self->node;
	self->size *= 2;
	self->lastfree = self->size - 1;
	self->node = malloc(sizeof(struct hash_node) * self->size);
	int i;
	for (i=0;i<self->size;i++) {
		self->node[i].hash = 0;
		self->node[i].key = NULL;
		self->node[i].next = -1;
	}
	for (i=0;i<osize;i++) {
		if (onode[i].key) {
			push_hashkey(self, onode[i].key, onode[i].hash, &onode[i].value , true);
		}
	}
	free(onode);
}

void
hash_push(struct hash * self , const char * key , size_t sz, struct data * value) {
	const char * str = store_string(self->temp, key, sz);
	uint32_t hash = calc_hash(str, sz);
	push_hashkey(self, str, hash, value , false);
};

struct data *
hash_search(struct hash * self, const char * key , size_t sz) {
	uint32_t h = calc_hash(key,sz);
	struct hash_node * n = &self->node[h & (self->size-1)];
	for (;;) {
		if (n->hash == h && memcmp(n->key , key , sz+1)==0) {
			return &n->value;
		}
		if (n->next < 0)
			return NULL;
		n = &self->node[n->next];
	}
};

static size_t
sum_string(lua_State *L) {
	size_t sz = 0;
	lua_pushnil(L);
	while (lua_next(L, STRING_INDEX) != 0) {
		size_t len = 0;
		lua_tolstring(L , -1 , &len);
		sz += (len+4) & ~3;
		lua_pop(L, 1);
	}	
	return sz;
}

static void
copy_string(lua_State *L , char * buffer) {
	lua_pushnil(L);
	while (lua_next(L, STRING_INDEX) != 0) {
		size_t len = 0;
		const char * str = lua_tolstring(L , -1 , &len);
		memcpy(buffer, str, len+1);
		lua_pop(L, 1);
		lua_pushvalue(L,-1);
		lua_createtable(L,2,0);
		lua_pushlightuserdata(L, buffer);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, len);
		lua_rawseti(L, -2, 2);
		lua_rawset(L, STRING_INDEX);
		buffer += (len+4) & ~3;
	}
}

static const char *
convert_string(lua_State *L, const char * old , size_t *sz) {
	lua_pushlightuserdata(L, (void *)old);
	lua_rawget(L, STRING_INDEX);
	lua_rawgeti(L, -1 , 1);
	const char * n = (const char *)lua_touserdata(L, -1);
	lua_pop(L,1);
	assert(n);
	if (sz) {
		lua_rawgeti(L, -1 , 2);
		*sz = (size_t)lua_tointeger(L, -1);
		lua_pop(L,1);
	}
	lua_pop(L,1);
	return n;
}

size_t
hash_genpool(struct hash * self) {
	assert(self->stringpool == NULL);
	lua_State *L = self->temp;
	size_t sz = sum_string(L);
	self->stringpool = (char *)malloc(sz);
	copy_string(L, self->stringpool);
	int i,j;
	for (i=0;i<self->size;i++) {
		struct hash_node * n = &(self->node[i]);
		if (n->key) {
			n->key = convert_string(L, n->key,NULL);
			switch(n->value.type) {
			case TYPE_STRING:
			case TYPE_FUNCTION:
				n->value.v.str.str = convert_string(L, n->value.v.str.str, &(n->value.v.str.len));
				break;
			case TYPE_TABLE:
				sz += sizeof(struct table) + n->value.v.tbl->size * sizeof(const char *);
				for (j=0;j<n->value.v.tbl->size;j++) {
					n->value.v.tbl->array[j] = convert_string(L, n->value.v.tbl->array[j],NULL);
				}
				break;
			}
		}
	}
	lua_close(L);
	self->temp = NULL;
	return sz + self->size * sizeof(struct hash_node) + sizeof(struct hash);
}

struct hash *
hash_new() {
	struct hash * self = malloc(sizeof(struct hash));
	self->size = 1;
	self->node = malloc(sizeof(struct hash_node));
	self->node[0].hash = 0;
	self->node[0].key = NULL;
	self->node[0].next = -1;
	self->lastfree = 0;
	self->stringpool = NULL;
	self->temp = luaL_newstate();
	lua_gc(self->temp, LUA_GCSTOP, 0);
	lua_newtable(self->temp);

	return self;
}

void
hash_delete(struct hash *self) {
	int i;
	for (i=0;i<self->size;i++) {
		struct hash_node *n = &self->node[i];
		if (n->key && n->value.type == TYPE_TABLE) {
			free(n->value.v.tbl);
		}
	}
	free(self->node);
	free(self->stringpool);
	if (self->temp) {
		lua_close(self->temp);
	}
}
