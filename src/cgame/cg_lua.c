
#include "cg_local.h"
#include "q_shared.h"
#include "cg_engine.h"
#include "json/cJSON.h"

#include "cg_lua.h"
#include "cg_luaserialiser.h"

#ifdef JPLUA

#pragma comment( lib, "lua" )

jplua_t JPLua = { 0 };

#define JPLUA_LOAD_CHUNKSIZE 1024

typedef struct gfd_s
{// JPLua File Data
	fileHandle_t f;
	int dataRemaining;
	char buff[JPLUA_LOAD_CHUNKSIZE];
} gfd_t;

void JPLua_DPrintf( const char *msg, ... )
{
#ifdef JPLUA_DEBUG
	va_list argptr;
	char text[1024] = { 0 };

	va_start( argptr, msg );
	Q_vsnprintf( text, sizeof( text ), msg, argptr );
	va_end( argptr );

	CG_Printf( "%s", text );
#endif
}

static const char *JPLua_LoadFile_Reader( lua_State *L, void *ud, size_t *sz )
{// Called by the loader, never access it directly!
	gfd_t *gfd = (gfd_t *)ud;

	if ( !gfd->dataRemaining )
		return NULL;

	if ( gfd->dataRemaining >= JPLUA_LOAD_CHUNKSIZE )
	{
		trap_FS_Read( gfd->buff, JPLUA_LOAD_CHUNKSIZE, gfd->f );
		gfd->dataRemaining -= JPLUA_LOAD_CHUNKSIZE;
		*sz = JPLUA_LOAD_CHUNKSIZE;
		return gfd->buff;
	}
	else
	{
		trap_FS_Read( gfd->buff, gfd->dataRemaining, gfd->f );
		*sz = gfd->dataRemaining;
		gfd->dataRemaining = 0;
		return gfd->buff;
	}
}

int JPLua_LoadFile( lua_State *L, const char *file )
{// Loads a file using JA's FS functions, only use THIS to load files into lua!
	fileHandle_t	f		= 0;
	int				len		= trap_FS_FOpenFile( file, &f, FS_READ );
	gfd_t			gfd;
	int				status;
	
	if ( !f || len <= 0 )
	{// File doesn't exist
		Com_Printf( "JPLua_LoadFile: Failed to load %s, file doesn't exist\n", file );
		return 1;
	}
	gfd.f = f;
	gfd.dataRemaining = len;
	
	

	status = (lua_load(L, JPLua_LoadFile_Reader, &gfd, va("@%s", file)) || lua_pcall(L,0,0,0));
	if ( status )
	{// Error occoured
		Com_Printf( "JPLua_LoadFile: Failed to load %s: %s\n", file, lua_tostring( L, -1 ) );
		lua_pop( L, 1 );
	}
	
	trap_FS_FCloseFile( f );
	return status;
}

int JPLua_StackDump (lua_State *L) {
	int i;
	int top = lua_gettop(L);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {

		case LUA_TSTRING:  /* strings */
			Com_Printf("`%s'", lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			Com_Printf(lua_toboolean(L, i) ? "true" : "false");
			break;

		case LUA_TNUMBER:  /* numbers */
			Com_Printf("%g", lua_tonumber(L, i));
			break;

		default:  /* other values */
			Com_Printf("%s", lua_typename(L, t));
			break;

		}
		Com_Printf("  ");  /* put a separator */
	}
	Com_Printf("\n");  /* end the listing */

	return 0;
}




// Framework functions constants
typedef enum jpLuaConsts_e {
	JPLUA_FRAMEWORK_TOSTRING,
	JPLUA_FRAMEWORK_PAIRS,
	JPLUA_FRAMEWORK_MAX,
} jpLuaConsts_t;

int JPLua_Framework[JPLUA_FRAMEWORK_MAX]; // Contains lua references to framework functions, if any of these are 0 after init, we got a serious problem

int JPLua_Push_ToString( lua_State *L )
{
	lua_rawgeti( L, LUA_REGISTRYINDEX, JPLua_Framework[JPLUA_FRAMEWORK_TOSTRING] );
	return 1;
}

int JPLua_Push_Pairs( lua_State *L )
{
	lua_rawgeti( L, LUA_REGISTRYINDEX, JPLua_Framework[JPLUA_FRAMEWORK_PAIRS] );
	return 1;
}

void JPLua_Util_ArgAsString( lua_State *L, char *out, int bufsize )
{
	char *msg;
//	char *nl;
	int args = lua_gettop(L);
	const char *res;
	int i;

	// Lets do this a lil different, concat all args and use that as the message ^^
	JPLua_Push_ToString( L ); // Ref to tostring (instead of a global lookup, in case someone changes it)

	for ( i=1; i<=args; i++ )
	{
		lua_pushvalue( L, -1 );
		lua_pushvalue( L, i );
		lua_call( L, 1, 1 ); // Assume this will never error out
		res = lua_tostring( L, -1 );
		if ( res )
			Q_strcat( out, bufsize, res );
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	msg = out;

	return;
	// It's messy but it works :P
	/*
	nl = msg;
	while ( 1 )
	{
		if ( !(*nl) )
		{
			if ( *msg )
			{
				assert( strlen( msg ) < 4096 ); // Failsafe, this should never happen (4096 is engine Com_Printf buffer)
				return;//Com_Printf( va( "%s\n", msg ) );
			}
			break;
		}
		if ( *nl == '\n' )
		{
		//	*nl = '\0';
			assert( strlen( msg ) < 4096 ); // Failsafe, this should never happen
			return;
			/*
			Com_Printf( va( "%s\n", msg ) );
			msg = nl + 1;
			*nl = '\n';
			*//*
		}
		nl++;
	}
	*/
}

void JPLua_TableToColour( vector4 *out, lua_State *L, int idx )
{
	int i=0;

	lua_pushnil( L );
	for ( i=0; i<4 && lua_next( L, idx ); i++ )
	{
		out->data[i] = lua_tonumber( L, -1 );
		lua_pop( L, 1 );
	}
}


static int JPLua_Export_Print( lua_State *L )
{
	char buf[15360] = { 0 };

	JPLua_Util_ArgAsString( L, buf, sizeof( buf ) );
	Com_Printf( buf );

	return 0;
}



static int JPLua_Export_Require( lua_State *L )
{
	const char *path = va( "lua/cl/%s", lua_tostring( L, 1 ) );
	JPLua_LoadFile( L, path );
	return 0;
}

static int JPLua_Export_DrawRect( lua_State *L )
{
	vector4 colour = { 1.0f };

	JPLua_TableToColour( &colour, L, 5 );

	CG_FillRect( (float)lua_tonumber( L, 1 ), (float)lua_tonumber( L, 2 ), (float)lua_tonumber( L, 3 ), (float)lua_tonumber( L, 4 ), &colour );
	return 0;
}

static int JPLua_Export_DrawText( lua_State *L )
{
	vector4 colour = { 1.0f };

	JPLua_TableToColour( &colour, L, 4 );

	CG_Text_Paint( (float)lua_tonumber( L, 1 ), (float)lua_tonumber( L, 2 ), (float)lua_tonumber( L, 5 ), &colour, lua_tostring( L, 3 ), 0.0f, 0, lua_tointeger( L, 6 ), lua_tointeger( L, 7 ) );

	return 0;
}

static int JPLua_Export_DrawPic( lua_State *L )
{
	int shader = 0;
	vector4 colour = { 1.0f };
	int height = 0, width = 0, y = 0, x = 0;

	shader		= (int)lua_tonumber( L, -1 );
	JPLua_TableToColour( &colour, L, -2 );
	height		= (int)lua_tonumber( L, -3 );
	width		= (int)lua_tonumber( L, -4 );
	y			= (int)lua_tonumber( L, -5 );
	x			= (int)lua_tonumber( L, -6 );

	trap_R_SetColor( &colour );
		CG_DrawPic( x, y, width, height, shader );
	trap_R_SetColor( NULL );

	return 0;
}

static int JPLua_Export_RegisterShader( lua_State *L )
{
	lua_pushinteger( L, trap_R_RegisterShader( lua_tostring( L, -1 ) ) );
	return 1;
}

extern void CG_ChatBox_AddString( char *chatStr ); //cg_draw.c
static int JPLua_Export_SendChatText( lua_State *L )
{
	char text[MAX_SAY_TEXT] = { 0 };

	if ( !cg_teamChatsOnly.integer ) {
		Q_strncpyz( text, lua_tostring( L, -1 ), MAX_SAY_TEXT );
		lua_pop( L, 1 );
		CG_LogPrintf( cg.log.console, va( "%s\n", text ) );
		if ( cg_newChatbox.integer )
			JP_ChatboxAdd( text, qfalse, "normal" );
		else
			CG_ChatBox_AddString( text );
	}

	return 0;
}

static int JPLua_Export_SendServerCommand( lua_State *L )
{
	trap_SendClientCommand( lua_tostring( L, -1 ) );
	return 0;
}

static int JPLua_Export_SendConsoleCommand( lua_State *L )
{
	trap_SendConsoleCommand( lua_tostring( L, -1 ) );
	return 0;
}


static int JPLua_Export_RegisterSound( lua_State *L )
{
	lua_pushinteger( L, trap_S_RegisterSound( lua_tostring( L, -1 ) ) );
	return 1;
}

static int JPLua_Export_StartLocalSound( lua_State *L )
{
	trap_S_StartLocalSound( lua_tonumber( L, -2 ), lua_tonumber( L, -1 ) );
	return 0;
}

static int JPLua_Export_RemapShader( lua_State *L )
{
	trap_R_RemapShader( lua_tostring( L, -3 ), lua_tostring( L, -2 ), lua_tostring( L, -1 ) );
	return 0;
}

static int JPLua_Export_ResolveHostname( lua_State *L )
{
#ifndef OPENJK
	netadr_t netAddr = { 0 };
#endif
	char buf[1024] = { 0 };

	JPLua_Util_ArgAsString( L, &buf[0], sizeof( buf ) );

	#ifdef OPENJK
		lua_pushnil( L );
		lua_pushstring( L, "^1ResolveHostname is not supported by this version of JA++" );
		return 2;
	#else
		if ( ENG_NET_StringToAddr( buf, &netAddr ) )
		{
			lua_pushfstring( L, "%i.%i.%i.%i (nettype: %i)", netAddr.ip[0], netAddr.ip[1], netAddr.ip[2], netAddr.ip[3], BigShort( netAddr.port ), netAddr.type );
			return 1;
		}
		else
		{
			lua_pushnil( L );
			lua_pushstring( L, "^1Could not resolve address" );
			return 2;
		}
	#endif
}

static int JPLua_Export_AddConsoleCommand( lua_State *L )
{
	jplua_plugin_command_t *cmd = JPLua.currentPlugin->consoleCmds;
	int funcType = lua_type( L, 2 );

	if ( lua_type( L, 1 ) != LUA_TSTRING || (funcType != LUA_TFUNCTION && funcType != LUA_TNIL) )
	{
		CG_Printf( "JPLua: AddConsoleCommand failed, function signature invalid registering %s (plugin: %s) - Is it up to date?\n", lua_tostring( L, -1 ), JPLua.currentPlugin->name );
		return 0;
	}

	while ( cmd && cmd->next)
		cmd = cmd->next;

	if ( cmd )
	{
		cmd->next = (jplua_plugin_command_t *)malloc( sizeof( jplua_plugin_command_t ) );
		memset( cmd->next, 0, sizeof( jplua_plugin_command_t ) );
		cmd = cmd->next;
	}
	else
	{
		JPLua.currentPlugin->consoleCmds = (jplua_plugin_command_t *)malloc( sizeof( jplua_plugin_command_t ) );
		memset( JPLua.currentPlugin->consoleCmds, 0, sizeof( jplua_plugin_command_t ) );
		cmd = JPLua.currentPlugin->consoleCmds;
	}

	Q_strncpyz( cmd->command, lua_tostring( L, 1 ), sizeof( cmd->command ) );
	if ( funcType != LUA_TNIL )
		cmd->handle = luaL_ref( L, LUA_REGISTRYINDEX );

	trap_AddCommand( cmd->command );
	return 0;
}

static int JPLua_Export_AddServerCommand( lua_State *L )
{
	jplua_plugin_command_t *cmd = JPLua.currentPlugin->serverCmds;

	if ( lua_type( L, -1 ) != LUA_TFUNCTION || lua_type( L, -2 ) != LUA_TSTRING )
	{
		CG_Printf( "JPLua: AddServerCommand failed, function signature invalid registering %s (plugin: %s) - Is it up to date?\n", lua_tostring( L, -1 ), JPLua.currentPlugin->name );
		return 0;
	}

	while ( cmd && cmd->next)
		cmd = cmd->next;

	if ( cmd )
	{
		cmd->next = (jplua_plugin_command_t *)malloc( sizeof( jplua_plugin_command_t ) );
		memset( cmd->next, 0, sizeof( jplua_plugin_command_t ) );
		cmd = cmd->next;
	}
	else
	{
		JPLua.currentPlugin->serverCmds = (jplua_plugin_command_t *)malloc( sizeof( jplua_plugin_command_t ) );
		memset( JPLua.currentPlugin->serverCmds, 0, sizeof( jplua_plugin_command_t ) );
		cmd = JPLua.currentPlugin->serverCmds;
	}

	Q_strncpyz( cmd->command, lua_tostring( L, -2 ), sizeof( cmd->command ) );
	cmd->handle = luaL_ref( L, LUA_REGISTRYINDEX );

	return 0;
}

static int JPLua_Export_Font_StringLengthPixels( lua_State *L )
{
//	lua_pushnumber( L, trap_R_Font_StrLenPixels( lua_tostring( L, 1 ), lua_tointeger( L, 2 ), (float)lua_tonumber( L, 3 ) ) );
	lua_pushnumber( L, CG_Text_Width( lua_tostring( L, 1 ), (float)lua_tonumber( L, 2 ), lua_tointeger( L, 3 ) ) );
	return 1;
}

static int JPLua_Export_Font_StringHeightPixels( lua_State *L )
{
//	lua_pushnumber( L, trap_R_Font_HeightPixels( lua_tointeger( L, 1 ), (float)lua_tonumber( L, 2 ) ) );
	lua_pushnumber( L, CG_Text_Height( lua_tostring( L, 1 ), (float)lua_tonumber( L, 2 ), lua_tointeger( L, 3 ) ) );
	return 1;
}

static int JPLua_Export_GetTime( lua_State *L )
{
	lua_pushinteger( L, cg.time );
	return 1;
}

static int JPLua_Export_GetMapTime( lua_State *L )
{
	int msec=0, secs=0, mins=0, limitSec=cgs.timelimit*60;

	msec = cg.time-cgs.levelStartTime;
	secs = msec/1000;
	mins = secs/60;

	if ( cgs.timelimit && (cg_drawTimer.integer & DRAWTIMER_COUNTDOWN) )
	{// count down
		msec = limitSec*1000 - (msec);
		secs = msec/1000;
		mins = secs/60;
	}

	secs %= 60;
	msec %= 1000;

	lua_pushinteger( L, mins );
	lua_pushinteger( L, secs );
	lua_pushinteger( L, msec );
	return 3;
}

static int JPLua_Export_GetRealTime( lua_State *L )
{
	lua_pushinteger( L, trap_Milliseconds() );
	return 1;
}

static int JPLua_Export_GetFPS( lua_State *L )
{
	lua_pushinteger( L, cg.japp.fps );
	return 1;
}

int JPLua_Export_Trace( lua_State *L )
{
	trace_t tr={0};
	vector3 start={0},end={0};
	float size=0;
	vector3 mins={0},maxs={0};
	int skipNumber=0,mask=0;
	int top=0, top2=0, top3=0;

	top = lua_gettop( L );

	lua_getfield( L, 1, "x" ); start.x = lua_tonumber( L, -1 );
	lua_getfield( L, 1, "y" ); start.y = lua_tonumber( L, -1 );
	lua_getfield( L, 1, "z" ); start.z = lua_tonumber( L, -1 );

	size = lua_tonumber( L, 2 ) / 2.0f;
	VectorSet( &mins, size, size, size );
	VectorScale( &mins, -1.0f, &maxs );

	lua_getfield( L, 3, "x" ); end.x = lua_tonumber( L, -1 );
	lua_getfield( L, 3, "y" ); end.y = lua_tonumber( L, -1 );
	lua_getfield( L, 3, "z" ); end.z = lua_tonumber( L, -1 );

	skipNumber = lua_tointeger( L, 4 );
	mask = lua_tointeger( L, 5 );

	CG_Trace( &tr, &start, &mins, &maxs, &end, skipNumber, mask );

	//CG_TestLine( start, end, 1000, 0, 1 );
	
	lua_newtable( L );
	top = lua_gettop( L );
	lua_pushstring( L, "allsolid" ); lua_pushboolean( L, !!tr.allsolid ); lua_settable( L, top );
	lua_pushstring( L, "startsolid" ); lua_pushboolean( L, !!tr.startsolid ); lua_settable( L, top );
	lua_pushstring( L, "entityNum" ); lua_pushinteger( L, tr.entityNum ); lua_settable( L, top );
	lua_pushstring( L, "fraction" ); lua_pushnumber( L, tr.fraction ); lua_settable( L, top );
	
	lua_pushstring( L, "endpos" );
		lua_newtable( L ); top2 = lua_gettop( L );
		lua_pushstring( L, "x" ); lua_pushnumber( L, tr.endpos.x ); lua_settable( L, top2 );
		lua_pushstring( L, "y" ); lua_pushnumber( L, tr.endpos.y ); lua_settable( L, top2 );
		lua_pushstring( L, "z" ); lua_pushnumber( L, tr.endpos.z ); lua_settable( L, top2 );
	lua_settable( L, top );

	lua_pushstring( L, "plane" );
		lua_newtable( L ); top2 = lua_gettop( L );
		lua_pushstring( L, "normal" );
			lua_newtable( L ); top3 = lua_gettop( L );
				lua_pushstring( L, "x" ); lua_pushnumber( L, tr.plane.normal.x ); lua_settable( L, top3 );
				lua_pushstring( L, "y" ); lua_pushnumber( L, tr.plane.normal.y ); lua_settable( L, top3 );
				lua_pushstring( L, "z" ); lua_pushnumber( L, tr.plane.normal.z ); lua_settable( L, top3 );
			lua_settable( L, top2 );
		lua_pushstring( L, "dist" ); lua_pushnumber( L, tr.plane.dist ); lua_settable( L, top2 );
		lua_pushstring( L, "type" ); lua_pushinteger( L, tr.plane.type ); lua_settable( L, top2 );
		lua_pushstring( L, "signbits" ); lua_pushinteger( L, tr.plane.signbits ); lua_settable( L, top2 );
	lua_settable( L, top );

	lua_pushstring( L, "surfaceFlags" ); lua_pushinteger( L, tr.surfaceFlags ); lua_settable( L, top );
	lua_pushstring( L, "contents" ); lua_pushinteger( L, tr.contents ); lua_settable( L, top );
	return 1;
}

static int JPLua_RegisterPlugin( lua_State *L )
{
	int top = 0;

	Q_strncpyz( JPLua.currentPlugin->name, lua_tostring( L, 1 ), sizeof( JPLua.currentPlugin->name ) );
	Q_CleanColorStr( JPLua.currentPlugin->name );
	Q_strncpyz( JPLua.currentPlugin->version, lua_tostring( L, 2 ), sizeof( JPLua.currentPlugin->version ) );
	Q_CleanColorStr( JPLua.currentPlugin->version );
	JPLua.currentPlugin->requiredJPLuaVersion = lua_isnumber( L, 3 ) ? lua_tointeger( L, 3 ) : JPLua.version;
	JPLua.currentPlugin->UID = (unsigned int)JPLua.currentPlugin;

	//lua_newtable( L );
	lua_newtable( L );
	top = lua_gettop( L );

	lua_pushstring( L, "name" );	lua_pushstring( L, JPLua.currentPlugin->name );		lua_settable( L, top );
	lua_pushstring( L, "version" );	lua_pushstring( L, JPLua.currentPlugin->version );	lua_settable( L, top );
	lua_pushstring( L, "UID" );		lua_pushinteger( L, JPLua.currentPlugin->UID );		lua_settable( L, top );

	//save in the registry, but push on stack again straight away
	JPLua.currentPlugin->handle = luaL_ref( L, LUA_REGISTRYINDEX );
	lua_rawgeti( L, LUA_REGISTRYINDEX, JPLua.currentPlugin->handle );

	return 1;
}


int JPLua_Export_TestLine( lua_State *L )
{
	vector3 start={0},end={0};
	float radius=0;
	int time=0;
	unsigned int color=0;

	lua_getfield( L, 1, "x" ); start.x = lua_tonumber( L, -1 );
	lua_getfield( L, 1, "y" ); start.y = lua_tonumber( L, -1 );
	lua_getfield( L, 1, "z" ); start.z = lua_tonumber( L, -1 );

	time = lua_tonumber( L, 3 );
	color = lua_tonumber( L, 4 );
	radius = lua_tonumber( L, 5 );
	lua_getfield( L, 2, "x" ); end.x = lua_tonumber( L, -1 );
	lua_getfield( L, 2, "y" ); end.y = lua_tonumber( L, -1 );
	lua_getfield( L, 2, "z" ); end.z = lua_tonumber( L, -1 );

	CG_TestLine( &start, &end, time, color, radius );

	return 0;
}



static const jplua_cimport_table_t JPLua_CImports[] =
{
	//Debugging
	{ "StackDump", JPLua_StackDump },

	//Interaction
	{ "RegisterPlugin", JPLua_RegisterPlugin }, // plugin RegisterPlugin( string name, string version )
	{ "AddListener", JPLua_Event_AddListener }, // AddListener( string name, function listener )
	{ "RemoveListener", JPLua_Event_RemoveListener }, // RemoveListener( string name )
	{ "GetSerialiser", JPLua_GetSerialiser }, // Serialiser GetSerialiser( string fileName )

	{ "AddConsoleCommand", JPLua_Export_AddConsoleCommand }, // AddConsoleCommand( string cmd )
	{ "AddServerCommand", JPLua_Export_AddServerCommand }, // AddServerCommand( string cmd )

	{ "GetPlayer", JPLua_GetPlayer }, // Player GetPlayer( integer clientNum )
	{ "GetServer", JPLua_GetServer }, // Server GetServer()
	{ "CreateCvar", JPLua_CreateCvar }, // Cvar CreateCvar( string name [, string value [, integer flags] ] )
	{ "GetCvar", JPLua_GetCvar }, // Cvar GetCvar( string name )

	//Resource management
	{ "RegisterShader", JPLua_Export_RegisterShader }, // integer RegisterShader( string path )
	{ "RegisterSound", JPLua_Export_RegisterSound }, // integer RegisterSound( string path )

	//Graphical
	{ "DrawRect", JPLua_Export_DrawRect }, // DrawRect( float x, float y, float width, float height, table { float r, float g, float b, float a } )
	{ "DrawText", JPLua_Export_DrawText }, // DrawText( float x, float y, string text, table { float r, float g, float b, float a }, float scale, integer fontStyle, integer fontIndex )
	{ "DrawPic", JPLua_Export_DrawPic }, // DrawPic( float x, float y, float width, float height, table { float r, float g, float b, float a }, integer shaderHandle )
	{ "Font_StringLengthPixels", JPLua_Export_Font_StringLengthPixels }, // integer Font_StringLengthPixels( string str, integer fontHandle, float scale )
	{ "Font_StringHeightPixels", JPLua_Export_Font_StringHeightPixels }, // integer Font_StringHeightPixels( integer fontHandle, float scale )

	//Communication
	{ "SendChatText", JPLua_Export_SendChatText }, // SendChatText( string text )
	{ "SendServerCommand", JPLua_Export_SendServerCommand }, // SendServerCommand( string command )
	{ "SendConsoleCommand", JPLua_Export_SendConsoleCommand }, // SendConsoleCommand( string command )

	//Misc
	{ "RemapShader", JPLua_Export_RemapShader }, // RemapShader( string oldshader, string newshader, string timeoffset )
	{ "StartLocalSound", JPLua_Export_StartLocalSound }, // StartLocalSound( integer soundHandle, integer channelNum )
	{ "ResolveHostname", JPLua_Export_ResolveHostname }, // string ResolveHostname( string hostname )
	{ "GetTime", JPLua_Export_GetTime }, // integer GetTime()
	{ "GetMapTime", JPLua_Export_GetMapTime }, // string GetMapTime()
	{ "GetRealTime", JPLua_Export_GetRealTime }, // integer GetRealTime()
	{ "GetFPS", JPLua_Export_GetFPS }, // integer GetFPS()
	{ "RayTrace", JPLua_Export_Trace }, // traceResult Trace( stuff )
	{ "TestLine", JPLua_Export_TestLine }, // traceResult Trace( stuff )
};

static const int cimportsSize = ARRAY_LEN( JPLua_CImports );


// Lua calls this if it panics, it'll then terminate the server with exit(EXIT_FAILURE)
// This error should never happen in a clean release version of JA++!
static int CGLuaI_Error( lua_State *L )
{
	CG_Printf( "^1*************** JA++ LUA ERROR ***************" );
	CG_Printf( "^1unprotected error in call to Lua API (%s)\n", lua_tostring(L,-1) );
	return 0;
}

#define JPLUA_DIRECTORY "lua/cl/"
#define JPLUA_EXTENSION ".lua"

static void JPLua_LoadPlugin( const char *pluginName, const char *fileName )
{
	if ( !JPLua.plugins )
	{//First plugin
		JPLua.plugins = (jplua_plugin_t *)malloc( sizeof( jplua_plugin_t ) );
		JPLua.currentPlugin = JPLua.plugins;
	}
	else
	{
		JPLua.currentPlugin->next = (jplua_plugin_t *)malloc( sizeof( jplua_plugin_t ) );
		JPLua.currentPlugin = JPLua.currentPlugin->next;
	}

	memset( JPLua.currentPlugin, 0, sizeof( jplua_plugin_t ) );
	Q_strncpyz( JPLua.currentPlugin->name, "<null>", sizeof( JPLua.currentPlugin->name ) );
	JPLua_LoadFile( JPLua.state, va( JPLUA_DIRECTORY"%s/%s", pluginName, fileName ) );

	if ( JPLua.currentPlugin->requiredJPLuaVersion > JPLua.version )
	{
		jplua_plugin_t *nextPlugin = JPLua.currentPlugin->next;
		CG_Printf( "%s requires JPLua version %i\n", pluginName, JPLua.currentPlugin->requiredJPLuaVersion );
		luaL_unref( JPLua.state, LUA_REGISTRYINDEX, JPLua.currentPlugin->handle );
		free( JPLua.currentPlugin );
		if ( JPLua.plugins == JPLua.currentPlugin )
			JPLua.plugins = nextPlugin;
		JPLua.currentPlugin = nextPlugin;
	}
	else
		CG_Printf( "%-15s%-32s%-8s%X\n", "Loaded plugin:", JPLua.currentPlugin->name, JPLua.currentPlugin->version, JPLua.currentPlugin->UID );
}

static void JPLua_PostInit( lua_State *L )
{
	char folderList[16384] = {0}, *folderName = NULL;
	int i=0, numFolders=0, folderLen=0;

	CG_Printf( "^5**************** ^3JA++ Lua (CL) is initialising ^5****************\n" );
	
	JPLua_LoadFile( L, JPLUA_DIRECTORY"init"JPLUA_EXTENSION );
	lua_getfield( L, LUA_GLOBALSINDEX, "JPLua" );
	lua_getfield( L, -1, "version" );
	JPLua.version = lua_tointeger( L, -1 );
	lua_pop( L, 1 );

	CG_Printf( "%-15s%-32s%-8s%s\n", "               ", "Name", "Version", "Unique ID" );

	numFolders = trap_FS_GetFileList( JPLUA_DIRECTORY, "/", folderList, sizeof( folderList ) );
	folderName = folderList;
	for ( i=0; i<numFolders; i++ )
	{
		Q_strstrip( folderName, "/\\", NULL );
		folderLen = strlen( folderName );
		//folderName[folderLen] = '\0'; // not necessary?
		if ( Q_stricmp( folderName, "." ) && Q_stricmp( folderName, ".." ) )
		{
			char fileList[16384] = {0}, *fileName = NULL;
			int j=0, numFiles=0, fileLen=0;
			
			numFiles = trap_FS_GetFileList( va( JPLUA_DIRECTORY"%s", folderName ), JPLUA_EXTENSION, fileList, sizeof( fileList ) );
			fileName = fileList;

			for ( j=0; j<numFiles; j++ )
			{
				Q_strstrip( fileName, "/\\", NULL );
				fileLen = strlen( fileName );
			//	fileName[fileLen] = '\0'; // not necessary?
				if ( !Q_stricmp( fileName, "plugin"JPLUA_EXTENSION ) )
				{
					JPLua_LoadPlugin( folderName, fileName );
					break;
				}
				fileName += fileLen+1;
			}
		}
		folderName += folderLen+1;
	}

	CG_Printf( "^5**************** ^2JA++ Lua (CL) is initialised ^5****************\n" );

	return;
}

void JPLua_Init( void )
{//Initialise the JPLua system
	int i = 0;

	if ( !cg_jplua.integer )
		return;

	//Initialise and load base libraries
	memset( JPLua_Framework, -1, sizeof( JPLua_Framework ) );
	JPLua.state = lua_open();
	if ( !JPLua.state )
	{//TODO: Fail gracefully
		return;
	}

	lua_atpanic( JPLua.state, CGLuaI_Error ); // Set the function called in a Lua error
	luaL_openlibs( JPLua.state );
	luaopen_string( JPLua.state );
	


	// Get rid of libraries we don't need
	lua_pushnil( JPLua.state );	lua_setglobal( JPLua.state, LUA_LOADLIBNAME ); // No need for the package library
	lua_pushnil( JPLua.state );	lua_setglobal( JPLua.state, LUA_IOLIBNAME ); // We use JKA engine facilities for file-handling

	// There are some stuff in the base library that we don't want
	lua_pushnil( JPLua.state );	lua_setglobal( JPLua.state, "dofile" );
	lua_pushnil( JPLua.state );	lua_setglobal( JPLua.state, "loadfile" );
	lua_pushnil( JPLua.state );	lua_setglobal( JPLua.state, "load" ); 
	lua_pushnil( JPLua.state );	lua_setglobal( JPLua.state, "loadstring" );

	// Some libraries we want, but not certain elements in them
	lua_getglobal( JPLua.state, LUA_OSLIBNAME ); // The OS library has dangerous access to the system, remove some parts of it
	lua_pushstring( JPLua.state, "execute" );		lua_pushnil( JPLua.state ); lua_settable( JPLua.state, -3 );
	lua_pushstring( JPLua.state, "exit" );			lua_pushnil( JPLua.state ); lua_settable( JPLua.state, -3 );
	lua_pushstring( JPLua.state, "remove" );		lua_pushnil( JPLua.state ); lua_settable( JPLua.state, -3 );
	lua_pushstring( JPLua.state, "rename" );		lua_pushnil( JPLua.state ); lua_settable( JPLua.state, -3 );
	lua_pushstring( JPLua.state, "setlocale" );		lua_pushnil( JPLua.state ); lua_settable( JPLua.state, -3 );
	lua_pushstring( JPLua.state, "tmpname" );		lua_pushnil( JPLua.state ); lua_settable( JPLua.state, -3 );
	lua_pop( JPLua.state, 1 );

	// Redefine global functions
	lua_pushcclosure( JPLua.state, JPLua_Export_Print, 0 );	lua_setglobal( JPLua.state, "print" );
	lua_pushcclosure( JPLua.state, JPLua_Export_Require, 0 );	lua_setglobal( JPLua.state, "require" );
	
	for ( i=0; i<cimportsSize; i++ )
		lua_register( JPLua.state, JPLua_CImports[i].name, JPLua_CImports[i].function );

	// Register our classes
	JPLua_Register_Player( JPLua.state );
	JPLua_Register_Server( JPLua.state );
	JPLua_Register_Cvar( JPLua.state );
	JPLua_Register_Serialiser( JPLua.state );

	// -- FRAMEWORK INITIALISATION begin
	lua_getglobal( JPLua.state, "tostring" );	JPLua_Framework[JPLUA_FRAMEWORK_TOSTRING]	= luaL_ref( JPLua.state, LUA_REGISTRYINDEX );
	lua_getglobal( JPLua.state, "pairs" );		JPLua_Framework[JPLUA_FRAMEWORK_PAIRS]		= luaL_ref( JPLua.state, LUA_REGISTRYINDEX );

	for ( i=0; i<JPLUA_FRAMEWORK_MAX; i++ )
	{
		if ( JPLua_Framework[i] < 0 )
			Com_Error( ERR_FATAL, "FATAL ERROR: Could not properly initialize the JPLua framework!\n" );
	}
	// -- FRAMEWORK INITIALISATION end


	//Call our base scripts
	JPLua_PostInit( JPLua.state );
}

void JPLua_Shutdown( void )
{
	if ( JPLua.state != NULL ) {
		jplua_plugin_t *nextPlugin = JPLua.plugins;

		JPLua_Event_Shutdown();

		JPLua.currentPlugin = JPLua.plugins;
		while ( nextPlugin )
		{
			luaL_unref( JPLua.state, LUA_REGISTRYINDEX, JPLua.currentPlugin->handle );
			nextPlugin = JPLua.currentPlugin->next;

			free( JPLua.currentPlugin );
			JPLua.currentPlugin = nextPlugin;
		}

		JPLua.plugins = JPLua.currentPlugin = NULL;

		lua_close( JPLua.state );
		JPLua.state = NULL;
	}
}

#endif // JPLUA
