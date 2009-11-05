
#include <sys/types.h>
#include <event.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* ----------- some utility functions ---------*/

/* turns end-relative indexes to [0..top] range */
static int fixindex (lua_State *L, int index) {
	int top = lua_gettop (L);
	
	if (index < 0 && -index <= top)
		index = top+index+1;
	
	return index;
}

/* missing luaL_optfunction() */
// static int kzL_optfunction (lua_State *L, int narg) {
// 	if (lua_isnoneornil(L, narg))
// 		return 0;
// 	luaL_checktype (L, narg, LUA_TFUNCTION);
// 	return 1;
// }

/* creates a luaL_ref from an optional argument */
static int ref_optarg (lua_State *L, int narg) {
	if (lua_isnoneornil (L, narg))
		return LUA_NOREF;
	
	lua_pushvalue (L, narg);
	return luaL_ref (L, LUA_REGISTRYINDEX);
}

/* turns a Lua "r"|"w"|"rw" string into EV_READ | EV_WRITE flags */
static short check_evtype (lua_State *L, int index) {
	short evt = 0;
	index = fixindex (L, index);
	size_t len;
	const char *evtyp_s = luaL_optlstring (L, index, "r", &len);
	while (len--) {
		switch (*evtyp_s++) {
			case 'r': evt |= EV_READ; break;
			case 'w': evt |= EV_WRITE; break;
		}
	}
	return evt;
}

/* ------------- event base object ----------- */

typedef struct lev_base {
	struct event_base *base;
	lua_State *L;
} lev_base;
#define EVENT_BASE	"levent base obj"
#define check_base(L,i)	((lev_base *)luaL_checkudata (L,i,EVENT_BASE))

/* -------------- event object --------------- */

typedef struct lev_event {
	struct event event;
	lev_base *base;
	int cb_ref;
} lev_event;
#define EVENT	"levent event obj"
#define check_ev(L,i)	((lev_event *)luaL_checkudata (L,i,EVENT))

/* -------------- bufferevent object ---------- */

typedef struct lev_buffered {
	struct bufferevent *buffered;
	lev_base *base;
	int fd;
	int readcb_ref;
	int writecb_ref;
	int errorcb_ref;
} lev_buffered;
#define BUFFEREVENT	"levent buffered event obj"
#define check_buffered(L,i)	((lev_buffered *)luaL_checkudata (L,i,BUFFEREVENT))


/* ------------- module functions ------------- */
/* creates an event base */
static int new_event_base (lua_State *L) {
// 	TODO: pasar a event_base_new(), depende libevent 1.4 o 2.0
// 	struct event_base *b = event_base_new();
	struct event_base *b = event_init();
	if (!b) {
		return luaL_error (L, "Can't create event base");
	}
	lev_base *lb = lua_newuserdata (L, sizeof (lev_base));
	lb->base = b;
	lb->L = L;
	
	luaL_getmetatable (L, EVENT_BASE);
	lua_setmetatable (L, -2);
	return 1;
}

static int g_fdacc_ref = LUA_NOREF;

/* setaccessor (mt, f) */
/* sets an fd accessor */
static int set_accessor (lua_State *L) {
	if (g_fdacc_ref == LUA_NOREF) {
		lua_newtable (L);
		g_fdacc_ref = luaL_ref (L, LUA_REGISTRYINDEX);
	}
															// (mt, f)
	lua_rawgeti (L, LUA_REGISTRYINDEX, g_fdacc_ref);		// (mf, f, fdacc)
	lua_insert (L, -2);										// (fdacc, mt, f)
	lua_rawset (L, -2);
	return 0;
}

/* gets an fd from any value */
static int fd_from_value (lua_State *L) {
	int fd = 0;
															// (v)
	if (lua_getmetatable (L, 1)) {							// (v mt)
		lua_rawgeti (L, LUA_REGISTRYINDEX, g_fdacc_ref);	// (v mt fdacc)
		if (lua_isnil (L, -1))
			return luaL_error (L, "undefined object type");
		lua_pushvalue (L, -2);								// (v mt fdacc mt)
		lua_rawget (L, -2);									// (v mt fdacc f)
		lua_pushvalue (L, 1);								// (v mt fdacc f v)
		lua_pcall (L, 1, 1, 0);								// (v mt fdacc r)
		return 1;
	} else {
		fd = lua_tointeger (L, 1);
		lua_pushinteger (L, fd);
		return 1;
	}
}

/* gets an fd from a file object */
static int fd_from_file (lua_State *L) {
	FILE **f = luaL_checkudata (L, 1, LUA_FILEHANDLE);
	lua_pushinteger (L, fileno (*f));
	return 1;
}


static const luaL_Reg module_functs[] = {
	{ "newbase", new_event_base },
	{ "fdfromfile", fd_from_file },
	{ "setfdaccessor", set_accessor },
	{ "fdfromvalue", fd_from_value },
	
	{ NULL, NULL },
};

/* ----------- event base methods --------------*/
static void event_cb (int fd, short evtype, void *v) {
	lev_event *ev = v;
	lua_State *L = ev->base->L;
	
	lua_rawgeti (L, LUA_REGISTRYINDEX, ev->cb_ref);
	lua_pushinteger (L, fd);
	lua_pcall (L, 1, 0, 0);
}

/* base:addevent (fd, callback, evtype) */
static int base_newevent (lua_State *L) {
	lev_base *b = check_base (L, 1);
	int fd = luaL_checkint (L, 2);
	luaL_checktype (L, 3, LUA_TFUNCTION);
	int cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);
	short evtype = check_evtype (L, 4);
	
	lev_event *ev = lua_newuserdata (L, sizeof (lev_event));
	event_set (&ev->event, fd, evtype, event_cb, ev);
	event_base_set (b->base, &ev->event);
	ev->base = b;
	ev->cb_ref = cb_ref;
// 	if (event_add (&ev->event, NULL) < 0) {
// 		return luaL_error (L, "mal event_add");
// 	}
	luaL_getmetatable (L, EVENT);
	lua_setmetatable (L, -2);
	return 1;
}



/* event buffer callbacks */

static void basic_bfrdcb (struct bufferevent *evbf, void *v) {
	lev_buffered *bf = v;
	lua_State *L = bf->base->L;
	if (bf->readcb_ref != LUA_NOREF) {
		lua_rawgeti (L, LUA_REGISTRYINDEX, bf->readcb_ref);
		lua_pushinteger (L, bf->fd);
		lua_pcall (L, 1, 0, 0);
	}
}
static void basic_bfwrcb (struct bufferevent *evbf, void *v) {
	lev_buffered *bf = v;
	lua_State *L = bf->base->L;
	if (bf->writecb_ref != LUA_NOREF) {
		lua_rawgeti (L, LUA_REGISTRYINDEX, bf->writecb_ref);
		lua_pushinteger (L, bf->fd);
		lua_pcall (L, 1, 0, 0);
	}
}
static void basic_bferrcb (struct bufferevent *evbf, short what, void *v) {
	lev_buffered *bf = v;
	lua_State *L = bf->base->L;
	if (bf->errorcb_ref != LUA_NOREF) {
		lua_rawgeti (L, LUA_REGISTRYINDEX, bf->errorcb_ref);
		lua_pushinteger (L, bf->fd);
		lua_pushinteger (L, what);
		lua_pcall (L, 2, 0, 0);
	}
}


/* base:newevbuf (fd, cb_read, cb_write, cb_error) */
static int base_newevbuf (lua_State *L) {
	lev_base *b = check_base (L, 1);
	int fd = luaL_checkint (L, 2);
	
	lev_buffered *bf = lua_newuserdata (L, sizeof (lev_buffered));
	bf->buffered = bufferevent_new (fd, basic_bfrdcb, basic_bfwrcb, basic_bferrcb, bf);
	// TODO: check allocation
	if (bufferevent_base_set (b->base, bf->buffered) < 0) {
		return luaL_error (L, "mal bufferevent_base_set");
	}
	bf->base = b;
	bf->fd = fd;
	bf->readcb_ref = ref_optarg (L, 3);
	bf->writecb_ref = ref_optarg (L, 4);
	bf->errorcb_ref = ref_optarg (L, 5);
	
	luaL_getmetatable (L, BUFFEREVENT);
	lua_setmetatable (L, -2);
	return 1;
}

/* base:loop ([times]) */
static int base_loop (lua_State *L) {
	lev_base *b = check_base (L, 1);
	
	int times = luaL_optinteger (L, 2, -1);
	int flags = 0;
	if (times > 0) flags |= EVLOOP_ONCE;
	if (times ==0) flags |= EVLOOP_NONBLOCK;
	
	int r = event_base_loop (b->base, flags);
	if (r < 0)
		return luaL_error (L, "mal event_base_loop");
	
	lua_pushboolean (L, 1);
	return 1;
}


static const luaL_Reg base_methods[] = {
	{ "newevent", base_newevent },
	{ "newevbuf", base_newevbuf },
	{ "loop", base_loop },
	
	{ NULL, NULL },
};

/* -------- event object methods ------------- */
static int event_start (lua_State *L) {
	lev_event *ev = check_ev (L, 1);
	if (event_add (&ev->event, NULL) < 0) {
		return luaL_error (L, "mal event_add");
	}
	return 1;
}

static int event_stop (lua_State *L) {
	lev_event *ev = check_ev (L, 1);
	if (event_del (&ev->event) < 0) {
		return luaL_error (L, "mal event_del");
	}
	return 1;
}

static int event_delete (lua_State *L) {
	lev_event *ev = check_ev (L, 1);
	luaL_unref (L, LUA_REGISTRYINDEX, ev->cb_ref);
	if (event_del (&ev->event) < 0)
		return luaL_error (L, "mal event_del");
	
	lua_pushboolean (L, 1);
	return 1;
}


static const luaL_Reg event_methods[] = {
	{ "start", event_start },
	{ "stop", event_stop },
	{ "delete", event_delete },
	{ "__gc", event_delete },
	
	{ NULL, NULL },
};

/* ----------- buffered event object methods ---------- */
static int evbuf_start (lua_State *L) {
	lev_buffered *bf = check_buffered (L, 1);
	short typ = ((bf->readcb_ref != LUA_NOREF) ? EV_READ : 0) |
				((bf->writecb_ref != LUA_NOREF) ? EV_WRITE : 0);
	if (bufferevent_enable (bf->buffered, typ) < 0)
		return luaL_error (L, "mal bufferevent_enable");
	return 1;
}

static int evbuf_stop (lua_State *L) {
	lev_buffered *bf = check_buffered (L, 1);
	if (bufferevent_disable (bf->buffered, EV_READ | EV_WRITE) < 0)
		return luaL_error (L, "mal bufferevent_disable");
	return 1;
}

#define MAXREAD		16384
static int evbuf_read (lua_State *L) {
	lev_buffered *bf = check_buffered (L, 1);
	size_t len = luaL_optinteger (L, 2, MAXREAD);
	
	struct evbuffer *ibuf = bf->buffered->input;
	if (len > ibuf->off)
		len = ibuf->off;
	
	lua_pushlstring (L, (const char *)ibuf->buffer, len);
	if (len)
		evbuffer_drain (ibuf, len);
	
	return 1;
}

static int evbuf_readline (lua_State *L) {
	lev_buffered *bf = check_buffered (L, 1);
	
	struct evbuffer *ibuf = bf->buffered->input;
	size_t len = ibuf->off;
	const unsigned char *p = ibuf->buffer;
	const unsigned char *end = ibuf->buffer + len;
	
	while (p < end && *p != '\r' && *p != '\n')
		p++;
	if (p >= end)
		return 0;
	
	lua_pushlstring (L, (const char *)ibuf->buffer, p-ibuf->buffer);
	
	if (p+1 < end) {
		char fch = *p, sch = *(p+1);
		if ((sch == '\r' || sch == '\n') && sch != fch)
			p++;
	}
	evbuffer_drain (ibuf, p-ibuf->buffer);
	return 1;
}

static int evbuf_write (lua_State *L) {
	lev_buffered *bf = check_buffered (L, 1);
	size_t len = 0;
	const char *data = luaL_checklstring (L, 2, &len);
	if (!data || !len || bufferevent_write (bf->buffered, data, len) < 0)
		return luaL_error (L, "mal bufferevent_write");
	lua_pop (L, 1);
	return 1;
}

static int evbuf_delete (lua_State *L) {
	lev_buffered *bf = check_buffered (L, 1);
	bufferevent_free (bf->buffered);
	bf->buffered = NULL;
	luaL_unref (L, LUA_REGISTRYINDEX, bf->readcb_ref);
	luaL_unref (L, LUA_REGISTRYINDEX, bf->writecb_ref);
	luaL_unref (L, LUA_REGISTRYINDEX, bf->errorcb_ref);
	
	lua_pushboolean (L, 1);
	return 1;
}
	

static const luaL_Reg evbuf_methods[] = {
	{ "start", evbuf_start },
	{ "stop", evbuf_stop },
	{ "read", evbuf_read },
	{ "readline", evbuf_readline },
	{ "write", evbuf_write },
	{ "delete", evbuf_delete },
	{ "__gc", evbuf_delete },
	
	{ NULL, NULL },
};

/* ----------- module setup ---------- */

static void meta_register (lua_State *L, const luaL_Reg *methods, const char *name) {
	luaL_newmetatable (L, name);
	lua_pushliteral (L, "__index");
	lua_pushvalue (L, -2);
	lua_rawset (L, -3);
	luaL_register (L, NULL, methods);
}

int luaopen_levent (lua_State *L);
int luaopen_levent (lua_State *L) {
	meta_register (L, base_methods, EVENT_BASE);
	meta_register (L, event_methods, EVENT);
	meta_register (L, evbuf_methods, BUFFEREVENT);
	
	luaL_register (L, "levent", module_functs);
	return 1;
}