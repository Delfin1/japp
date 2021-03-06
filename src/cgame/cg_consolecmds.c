// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"
#include "bg_saga.h"
#include "cg_engine.h"
#if MAC_PORT
	#include "macosx/jp_mac.h"
#endif
#include "cg_lua.h"

void CG_TargetCommand_f( void ) {
	int		targetNum;
	char	test[4];

	targetNum = CG_CrosshairPlayer();
	if (!targetNum ) {
		return;
	}

	trap_Argv( 1, test, 4 );
	trap_SendConsoleCommand( va( "gc %i %i", targetNum, atoi( test ) ) );

}

static void CG_SizeUp_f (void) {
	trap_Cvar_Set("cg_viewSize", va("%i",(int)(cg_viewSize.integer+10)));
}

static void CG_SizeDown_f (void) {
	trap_Cvar_Set("cg_viewSize", va("%i",(int)(cg_viewSize.integer-10)));
}

static void CG_Viewpos_f (void) {
	refdef_t *refdef = CG_GetRefdef();
	CG_Printf( "%s (%i %i %i) : %i\n", cgs.mapname, (int)refdef->vieworg.x, (int)refdef->vieworg.y, (int)refdef->vieworg.z, (int)refdef->viewangles.yaw );
}

void CG_ScoresDown_f( void ) {

	CG_BuildSpectatorString();
	if ( cg.scoresRequestTime + 2000 < cg.time ) {
		// the scores are more than two seconds out of data,
		// so request new ones
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );

		// leave the current scores up if they were already
		// displayed, but if this is the first hit, clear them out
		if ( !cg.showScores ) {
			cg.showScores = qtrue;
		//	cg.numScores = 0;
		}
	} else {
		// show the cached contents even if they just pressed if it
		// is within two seconds
		cg.showScores = qtrue;
	}
}

static void CG_ScoresUp_f( void ) {
	if ( cg.showScores ) {
		cg.showScores = qfalse;
		cg.scoreFadeTime = cg.time;
	}
}

static void CG_spWin_f( void) {
	trap_Cvar_Set("cg_cameraOrbit", "2");
	trap_Cvar_Set("cg_cameraOrbitDelay", "35");
	trap_Cvar_Set("cg_thirdPerson", "1");
	trap_Cvar_Set("cg_thirdPersonAngle", "0");
	trap_Cvar_Set("cg_thirdPersonRange", "100");
	CG_AddBufferedSound(cgs.media.winnerSound);
	//trap_S_StartLocalSound(cgs.media.winnerSound, CHAN_ANNOUNCER);
	CG_CenterPrint(CG_GetStringEdString("MP_INGAME", "YOU_WIN"), SCREEN_HEIGHT * .30, 0);
}

static void CG_spLose_f( void) {
	trap_Cvar_Set("cg_cameraOrbit", "2");
	trap_Cvar_Set("cg_cameraOrbitDelay", "35");
	trap_Cvar_Set("cg_thirdPerson", "1");
	trap_Cvar_Set("cg_thirdPersonAngle", "0");
	trap_Cvar_Set("cg_thirdPersonRange", "100");
	CG_AddBufferedSound(cgs.media.loserSound);
	//trap_S_StartLocalSound(cgs.media.loserSound, CHAN_ANNOUNCER);
	CG_CenterPrint(CG_GetStringEdString("MP_INGAME", "YOU_LOSE"), SCREEN_HEIGHT * .30, 0);
}

static void CG_TellTarget_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 )
		return;

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_TellAttacker_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_LastAttacker();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_StartOrbit_f( void ) {
	char var[MAX_TOKEN_CHARS];

	trap_Cvar_VariableStringBuffer( "developer", var, sizeof( var ) );
	if ( !atoi(var) )
		return;

	if (cg_cameraOrbit.value != 0) {
		trap_Cvar_Set ("cg_cameraOrbit", "0");
		trap_Cvar_Set("cg_thirdPerson", "0");
	} else {
		trap_Cvar_Set("cg_cameraOrbit", "5");
		trap_Cvar_Set("cg_thirdPerson", "1");
		trap_Cvar_Set("cg_thirdPersonAngle", "0");
		trap_Cvar_Set("cg_thirdPersonRange", "100");
	}
}

void CG_SiegeBriefingDisplay( int team, int dontshow );
static void CG_SiegeBriefing_f( void ) {
	int team;

	if (cgs.gametype != GT_SIEGE)
		return;

	team = cg.predictedPlayerState.persistant[PERS_TEAM];
	if (team != SIEGETEAM_TEAM1 && team != SIEGETEAM_TEAM2)
		return;

	CG_SiegeBriefingDisplay(team, 0);
}

static void CG_SiegeCvarUpdate_f( void ) {
	int team;

	if (cgs.gametype != GT_SIEGE)
		return;

	team = cg.predictedPlayerState.persistant[PERS_TEAM];

	if (team != SIEGETEAM_TEAM1 && team != SIEGETEAM_TEAM2)
		return;

	CG_SiegeBriefingDisplay(team, 1);
}

static void CG_SiegeCompleteCvarUpdate_f( void ) {
	if (cgs.gametype != GT_SIEGE)
	{ //Cannot be displayed unless in this gametype
		return;
	}

	// Set up cvars for both teams
	CG_SiegeBriefingDisplay(SIEGETEAM_TEAM1, 1);
	CG_SiegeBriefingDisplay(SIEGETEAM_TEAM2, 1);
}

static void CG_CopyNames_f( void ) {
#ifdef _WIN32
	char far	*buffer;
	int			bytes = 0;
	HGLOBAL		clipbuffer;
	int			i;
	char		buf[1216] = {0};	//(32*36) = 1152

	memset( buf, 0, sizeof( buf ) );
	buf[0] = '\0';

	for ( i=0; i<MAX_CLIENTS; i++ )
	{
		char *toClip = NULL;
		if ( cgs.clientinfo[i].infoValid ) {
			toClip = cgs.clientinfo[i].name;
			Q_strcat( buf, sizeof( buf ), va( "%s\n", toClip ) );
		}
	}
	Q_strcat( buf, sizeof( buf ), "\r\n" );	//Clipboard requires CRLF ending
	bytes = strlen( buf );

	OpenClipboard( NULL );
	EmptyClipboard();

	clipbuffer	= GlobalAlloc( GMEM_DDESHARE, bytes+1 );
	buffer		= (char far *)GlobalLock( clipbuffer ); // 'argument 1' might be '0': this does not adhere to the specification for the function 'GlobalLock'

	if ( !buffer )
		return;// GetLastError() * -1; // Do what you want to signal error

	strcpy( buffer, buf );

	GlobalUnlock( clipbuffer );
	SetClipboardData( CF_TEXT, clipbuffer );
	CloseClipboard();
#else //WIN32
	CG_Printf( "Feature not yet implemented for your operating system (copynames)\n" );
#endif //WIN32
}

static void CG_ShowPlayerID_f( void ) {
	int i;
	for ( i=0; i<MAX_CLIENTS; i++ ) {
		if ( cgs.clientinfo[i].infoValid )
			Com_Printf( "^7(^5%i^7) %s\n", i, cgs.clientinfo[i].name );
	}
}

static const char *pluginDisableStrings[] = {
	"Plugin_NewDrainEFX_needReconnection",
	"Plugin_IgnoreAllPlayers_WhenInDuel",
	"PLugin_EndDuel_Rotation",
	"Plugin_seeBlackSabers_needUserInfoUpdate",
	"Plugin_AutoReplier",
	"Plugin_NewForceEffect",
	"Plugin_No_NewDeathMessage",
	"Plugin_NewForceSight_effect",
	"Plugin_NoAltDim_effect",
	"Plugin_holsteredSaberBolted",
	"Plugin_ledgeGrab",
	"Plugin_NewDFA_PrimAttack",
	"Plugin_NewDFA_AltAttack",
	"Plugin_No_SP_Cartwheel",
	"Plugin_AllowDownloadURL_Redirect"
};
static const int numPluginDisableOpts = ARRAY_LEN( pluginDisableStrings );

// 0x30025430 JA+ cgame 1.4 beta3
static void CG_PluginDisable_f( void ) {
	int i;
	if ( trap_Argc() > 1 )
	{
		char arg[8]={0}, buf[16]={0};
		int current, toggle, index;
		trap_Argv( 1, arg, sizeof( arg ) );
		index = toggle = atoi( arg );
		if ( toggle < 0 || toggle > numPluginDisableOpts-1 ) {
			Com_Printf( "Invalid pluginDisable value: %i\n", toggle );
			return;
		}

		trap_Cvar_VariableStringBuffer( "cp_pluginDisable", buf, sizeof( buf ) );
		current = atoi( buf );
		toggle = (1<<index);
		trap_Cvar_Set( "cp_pluginDisable", va( "%i", toggle ^ current ) );

		Com_Printf( "%s %s\n", pluginDisableStrings[index], ((current&toggle)?"^2Allowed":"^1Disallowed") );
	}
	else
	{
		char buf[16]={0};
		trap_Cvar_VariableStringBuffer( "cp_pluginDisable", buf, sizeof( buf ) );

		Com_Printf( "Usage: /pluginDisable <ID>\n" );
		for ( i=0; i<numPluginDisableOpts; i++ ) {
			qboolean allowed = !(atoi( buf ) & (1<<i));
			Com_Printf( "^7(^5%i^7) ^%c%s\n", i, (allowed?'2':'1'), pluginDisableStrings[i] );
		}
	}
}

static void CG_ScrollChat_f( void ) {
	char args[8]={0};
	trap_Args( args, sizeof( args ) );
	JP_ChatboxScroll( atoi( args ) );
}

#ifndef OPENJK
	#ifdef _WIN32
		static void (*Con_Clear_f)( void ) = (void (*)( void ))0x4171A0;
	#elif defined(MAC_PORT)
	#endif
#else
	static void (*Con_Clear_f)( void ) = (void (*)( void ))0x00000000;
#endif // !OPENJK

static void CG_Clear_f( void ) {
	JP_ChatboxClear();
	if ( Con_Clear_f )
		Con_Clear_f();
}

static void CG_Menu_f( void ) {
	trap_Key_SetCatcher( trap_Key_GetCatcher() ^ KEYCATCH_CGAME );
}

static void Cmd_ChatboxSelectTabNextNoKeys( void ) {
	JP_ChatboxSelectTabNextNoKeys();
}
static void Cmd_ChatboxSelectTabPrevNoKeys( void ) {
	JP_ChatboxSelectTabPrevNoKeys();
}

static void CG_ChatboxFindTab_f( void ) {
	char args[128];
	trap_Args( args, sizeof( args ) );
	JP_ChatboxSelect( args );
}

#ifdef JPLUA

void CG_LuaDoString_f( void )
{
	char arg[MAX_TOKEN_CHARS];
	char buf[4096]={0};
	int i=0;
	int argc = trap_Argc();

	if ( argc < 2 || !JPLua.state )
		return;

	for ( i=1; i<argc; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		Q_strcat( buf, sizeof(buf), va( "%s ", arg ) );
	}

	if ( trap_Key_GetCatcher() & KEYCATCH_CONSOLE )
		CG_Printf( "^5Executing Lua code...\n" );
	if ( luaL_dostring( JPLua.state, buf ) != 0 )
		CG_Printf( "^1Lua Error: %s\n", lua_tostring( JPLua.state, -1 ) );
}

void CG_LuaReload_f( void ) {
	CG_Printf( "^5Reloading JPLua...\n" );
	JPLua_Shutdown();
	JPLua_Init();
}
#endif // JPLUA

#ifdef R_POSTPROCESSING
	#include "tr_ext_public.h"
	void CG_PostProcess_Reload_f( void ) {
		R_EXT_Cleanup();
		R_EXT_Init();
	}
#endif // R_POSTPROCESSING

//cg_consolecmds.c

void CG_FixDirection(void){
	if (cg.japp.isfixedVector){
		cg.japp.isfixedVector = qfalse;
		CG_Printf("Direction unset.\n");
		return;
	}

	if (trap_Argc() == 3){
		cg.japp.fixedVector.x = atof( CG_Argv( 1 ) );
		cg.japp.fixedVector.y = atof( CG_Argv( 2 ) );
	} else {
		AngleVectors( &cg.predictedPlayerState.viewangles, &cg.japp.fixedVector, NULL, NULL);
	}

	cg.japp.fixedVector.z = 0;
	cg.japp.isfixedVector = qtrue;
	CG_Printf( "Direction set (%.3f,%.3f).\n", cg.japp.fixedVector.x, cg.japp.fixedVector.y );
}

void CG_FakeGun_f( void ) {
	cg.japp.fakeGun = !cg.japp.fakeGun;
}

void CG_SayTeam_f( void ) {
	char buf[MAX_TOKEN_CHARS] = {0};
	trap_Args( buf, sizeof( buf ) );
	HandleTeamBinds( buf, sizeof( buf ) );
	trap_SendClientCommand( va( "say_team %s", buf ) );
}

#include "ui/ui_shared.h"
static void CG_HudReload_f( void ) {
	char *hudSet = NULL;

	String_Init();
	Menu_Reset();

	hudSet = cg_hudFiles.string;
	if ( hudSet[0] == '\0' ) 
		hudSet = "ui/jahud.txt";

	CG_LoadMenus( hudSet );
}


typedef struct command_s {
	char	*name;
	void	(*func)( void );
} command_t;

static command_t commands[] = {
	{ "+scores",					CG_ScoresDown_f },
	{ "-scores",					CG_ScoresUp_f },
	{ "addbot",						NULL },
	{ "bot_order",					NULL },
	{ "briefing",					CG_SiegeBriefing_f },
	{ "callvote",					NULL },
	{ "cgmenu",						CG_Menu_f },
	{ "chattabfind",				CG_ChatboxFindTab_f },
	{ "chattabnext",				Cmd_ChatboxSelectTabNextNoKeys },
	{ "chattabprev",				Cmd_ChatboxSelectTabPrevNoKeys },
	{ "clear",						CG_Clear_f },
	{ "copynames",					CG_CopyNames_f },
	{ "engage_duel",				NULL },
	{ "fakegun",					CG_FakeGun_f },
	{ "follow",						NULL },
	{ "force_absorb",				NULL },
	{ "force_distract",				NULL },
	{ "force_forcepowerother",		NULL },
	{ "force_heal",					NULL },
	{ "force_healother",			NULL },
	{ "force_protect",				NULL },
	{ "force_pull",					NULL },
	{ "force_rage",					NULL },
	{ "force_seeing",				NULL },
	{ "force_speed",				NULL },
	{ "force_throw",				NULL },
	{ "forcechanged",				NULL },
	{ "forcenext",					CG_NextForcePower_f },
	{ "forceprev",					CG_PrevForcePower_f },
	{ "give",						NULL },
	{ "god",						NULL },
	{ "hud_reload",					CG_HudReload_f },
	{ "invnext",					CG_NextInventory_f },
	{ "invprev",					CG_PrevInventory_f },
	{ "kill",						NULL },
	{ "levelshot",					NULL },
	{ "loaddefered",				NULL },
	{ "loaddeferred",				CG_LoadDeferredPlayers },
#ifdef JPLUA
	{ "lua",						CG_LuaDoString_f },
	{ "lua_reload",					CG_LuaReload_f },
#endif // JPLUA
	{ "nextframe",					CG_TestModelNextFrame_f },
	{ "nextskin",					CG_TestModelNextSkin_f },
	{ "noclip",						NULL },
	{ "notarget",					NULL },
	{ "npc",						NULL },
	{ "pluginDisable",				CG_PluginDisable_f },
#ifdef R_POSTPROCESSING
	{ "postprocess_reload",			CG_PostProcess_Reload_f },
#endif // R_POSTPROCESSING
	{ "prevframe",					CG_TestModelPrevFrame_f },
	{ "prevskin",					CG_TestModelPrevSkin_f },
	{ "saberAttackCycle",			NULL },
	{ "say",						NULL },
	{ "say_team",					CG_SayTeam_f },
	{ "scrollChat",					CG_ScrollChat_f },
	{ "setviewpos",					NULL },
	{ "showPlayerID",				CG_ShowPlayerID_f },
	{ "siegeCompleteCvarUpdate",	CG_SiegeCompleteCvarUpdate_f },
	{ "siegeCvarUpdate",			CG_SiegeCvarUpdate_f },
	{ "sizedown",					CG_SizeDown_f },
	{ "sizeup",						CG_SizeUp_f },
	{ "sm_fix_direction",			CG_FixDirection },
	{ "stats",						NULL },
	{ "sv_forcenext",				NULL },
	{ "sv_forceprev",				NULL },
	{ "sv_invnext",					NULL },
	{ "sv_invprev",					NULL },
	{ "sv_saberswitch",				NULL },
	{ "tcmd",						CG_TargetCommand_f },
	{ "team",						NULL },
	{ "teamtask",					NULL },
	{ "tell",						NULL },
	{ "tell_attacker",				CG_TellAttacker_f },
	{ "tell_target",				CG_TellTarget_f },
	{ "testgun",					CG_TestGun_f },
	{ "testmodel",					CG_TestModel_f },
	{ "use_bacta",					NULL },
	{ "use_electrobinoculars",		NULL },
	{ "use_field",					NULL },
	{ "use_seeker",					NULL },
	{ "use_sentry",					NULL },
	{ "viewpos",					CG_Viewpos_f },
	{ "vote",						NULL },
	{ "weapnext",					CG_NextWeapon_f },
	{ "weapon",						CG_Weapon_f },
	{ "weaponclean",				CG_WeaponClean_f },
	{ "weapprev",					CG_PrevWeapon_f },
	{ "zoom",						NULL },
};
static size_t numCommands = ARRAY_LEN( commands );

static int cmdcmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((command_t*)b)->name );
}

// The string has been tokenized and can be retrieved with Cmd_Argc() / Cmd_Argv()
qboolean CG_ConsoleCommand( void ) {
	const char *cmd = NULL;
	command_t *command = NULL;

	if ( JPLua_Event_ConsoleCommand() )
		return qtrue;

	cmd = CG_Argv( 0);

	command = (command_t *)bsearch( cmd, commands, numCommands, sizeof( commands[0] ), cmdcmp );
	if ( !command )
		return qfalse;

//	Com_Printf( "Executing %s\n", command->name );

	if ( !command->func )
		return qfalse;

	command->func();
	return qtrue;
}

void CG_InitConsoleCommands( void ) {
	command_t *cmd = commands;
	size_t i;

	for ( i=0; i<numCommands; i++, cmd++ )
		trap_AddCommand( cmd->name );
}
