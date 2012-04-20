#include <pthread.h>
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>

#define MAX 10

static lua_State *
_new(const char * name) {
	lua_State * L = luaL_newstate();
	lua_gc(L, LUA_GCSTOP, 0);
	luaL_openlibs(L);  
	lua_gc(L, LUA_GCRESTART, 0);
	int err = luaL_dofile(L, name);
	assert(err == 0);
	return L;
}

static void *
thread_test(void* ptr) { 
	lua_State * L = _new("test.lua");
	lua_close(L);
	return NULL;
}

int 
main() {
	lua_State * L = _new("test.lua");

	printf("main start\n");

	pthread_t pid[MAX];

	int i;

	for (i=0;i<MAX;i++) {
		pthread_create(&pid[i], NULL, thread_test, NULL);
	}

	for (i=0;i<MAX;i++) {
		pthread_join(pid[i], NULL); 
	}

	lua_close(L);	
	printf("main exit\n");
	return 0;
}