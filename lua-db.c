#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <string.h>
#include <assert.h>

#include "hash.h"

static struct hash * g_Hash = NULL;
static int g_Init = 0;

#define DATA 1
#define INDEX 2

static void
push_table(lua_State *L, struct hash * h, const char * key, size_t sz) {
	size_t n = lua_rawlen(L, -1);
	struct table tbl;
	tbl.size = (int)n;
	const char * data[n];
	tbl.array = data;
	int i;
	for (i=0;i<tbl.size;i++) {
		lua_rawgeti(L, -1 , i+1);
		assert(lua_type(L, -1) == LUA_TSTRING);
		data[i] = lua_tostring(L, -1);
		lua_pop(L,1);
	}

	struct data d;
	d.type = TYPE_TABLE;
	d.v.tbl = &tbl;

	hash_push(h, key, sz, &d);
}

static struct hash *
convert_hash(lua_State *L , size_t *sz) {
	struct hash * h = hash_new();
	lua_pushnil(L);
	struct data d;
	while (lua_next(L, INDEX) != 0) {
		if (lua_type(L,-2) == LUA_TSTRING) {
			size_t sz = 0;
			const char * key = lua_tolstring(L,-2,&sz);
			switch(lua_type(L,-1)) {
			case LUA_TNIL:
				d.type = TYPE_NIL;
				hash_push(h, key, sz, &d);
				break;
			case LUA_TSTRING:
				d.type = TYPE_STRING;
				d.v.str.str = lua_tolstring(L,-1,&d.v.str.len);
				hash_push(h, key, sz, &d);
				break;
			case LUA_TBOOLEAN:
				d.type = TYPE_BOOLEAN;
				d.v.boolean = lua_toboolean(L,-1);
				hash_push(h, key, sz, &d);
				break;
			case LUA_TNUMBER:
				d.type = TYPE_NUMBER;
				d.v.n = lua_tonumber(L,-1);
				hash_push(h, key, sz, &d);
				break;
			case LUA_TFUNCTION:
				lua_rawget(L,INDEX);
				d.type = TYPE_FUNCTION;
				d.v.str.str = lua_tolstring(L,-1,&d.v.str.len);
				hash_push(h, key, sz, &d);
				break;
			case LUA_TTABLE:
				push_table(L,h, key, sz);
				break;
			default:
				luaL_error(L , "table has unsupport type : %s", lua_typename(L,lua_type(L,-1)));
				break;
			}
		}
		lua_pop(L,1);
	}
	*sz = hash_genpool(h);

	return h;
}

static size_t
_init(lua_State *L) {
	const char * loader = luaL_checkstring(L,1);
	lua_State * dbL = luaL_newstate();
	lua_gc(dbL, LUA_GCSTOP, 0);
	luaL_openlibs(dbL);  
	lua_gc(dbL, LUA_GCRESTART, 0);

	int top = lua_gettop(dbL);
	int err = luaL_dofile(dbL, loader);
	if (err) {
		luaL_error(L, "%s" , lua_tostring(dbL,-1));
	}
	lua_settop(dbL, top+1);
	lua_insert(dbL, DATA);

	if (lua_type(dbL, DATA) != LUA_TTABLE) {
		lua_close(dbL);
		luaL_error(L, "%s need return a table", loader);
	}

	lua_newtable(dbL);
	lua_rawgeti(dbL, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	lua_setfield(dbL, -2, "__index");
	lua_setmetatable(dbL, DATA);

	const char * posloader = luaL_checkstring(L,2);

	err = luaL_loadstring(dbL, posloader);
	if (err) {
		lua_close(dbL);
		luaL_error(L, "load posload error");
	}

	lua_pushvalue(dbL,DATA);
	lua_setupvalue(dbL, -2, 1);  // set DATA to _ENV of posloader 

	err = lua_pcall(dbL, 0, 1, 0);

	if (err) {
		lua_close(dbL);
		luaL_error(L, "posload : %s" , lua_tostring(dbL,-1));
	}

	lua_insert(dbL, INDEX);

	if (lua_type(dbL, INDEX) != LUA_TTABLE) {
		lua_close(dbL);
		luaL_error(L, "posload need return a table");
	}

	lua_pushstring(dbL, loader);
	lua_setfield(dbL, LUA_REGISTRYINDEX , "loader");

	lua_settop(dbL, 2);

	size_t sz = 0;

	g_Hash = convert_hash(dbL, &sz);

	lua_close(dbL);

	return sz;
}


static int
db_open(lua_State *L) {
	int init = __sync_val_compare_and_swap (&g_Init, 0, 1);
	if (init == 0) {
		size_t sz = _init(L);
		lua_pushinteger(L, sz);
		return 1;
	} else {
		if (g_Hash == NULL) {
			return luaL_error(L, "Global database init failed");
		}
	}
	return 0;
}

static int
db_get(lua_State *L) {
	assert(g_Hash);
	size_t sz = 0;
	const char * key = luaL_checklstring(L,1,&sz);
	struct data * d = hash_search(g_Hash , key, sz);
	if (d==NULL) {
		luaL_error(L, "%s is not exist", key);
	}

	switch(d->type) {
	case TYPE_NIL:
		return 0;
	case TYPE_NUMBER:
		lua_pushnumber(L, d->v.n);
		return 1;
	case TYPE_STRING:
		lua_pushlstring(L, d->v.str.str, d->v.str.len);
		return 1;
	case TYPE_BOOLEAN:
		lua_pushboolean(L, d->v.boolean);
		return 1;
	case TYPE_FUNCTION:
		lua_pushlightuserdata(L, d);
		lua_pushliteral(L,"function");
		return 2;
	case TYPE_TABLE:
		lua_pushlightuserdata(L,d);
		lua_pushliteral(L,"table");
		return 2;
	}
	return luaL_error(L, "unknown type");
}

static int
db_expend(lua_State *L) {
	struct data *d = lua_touserdata(L,1);
	assert(d);
	int i;
	switch (d->type) {
	case TYPE_FUNCTION:
		lua_pushlstring(L, d->v.str.str, d->v.str.len);
		break;
	case TYPE_TABLE:
		lua_createtable(L, d->v.tbl->size , 0);
		for (i=0;i<d->v.tbl->size;i++) {
			lua_pushstring(L, d->v.tbl->array[i]);
			lua_rawseti(L,-2,i+1);
		}
		break;
	default:
		return luaL_error(L, "unknown type");
	}
	return 1;
}

int
luaopen_database_c(lua_State *L) {
	luaL_Reg l[] = {
		{ "open" , db_open },
		{ "get" , db_get },
		{ "expend" , db_expend },
		{ NULL, NULL },
	};
	luaL_checkversion(L);
	luaL_newlib(L,l);
	return 1;
}
