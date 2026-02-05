/***
*
*  Copyright (c) 1999, Valve LLC. All rights reserved.
*
*  This product contains software technology licensed from Id 
*  Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*  All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
///////////////////////////////////////////////////////////////////////////////////////////////
//
// This file contains both GNU as VALVE licenced material.
//
// This source file contains VALVE LCC licence material.
// Read and Agree to the above stated header
// before distibuting or editing this source code.
//
// Please read and agree to the mb_gnu_license.txt file
// (the file is located in the marine_bot source folder)
// before editing or distributing this source code.
// This source code is free for use under the rules of the GNU General Public License.
// For more information goto:: http://www.gnu.org/licenses/
//
// credits to - valve, botman.
//
// Marine Bot - code by Frank McNeil, Kota@, Mav, Shrike.
//
// (http://marinebot.xf.cz)
//
//
// util.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(WIN32)
#pragma warning(disable: 4996)
#endif

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"
#include "engine.h"

#pragma warning( default: 4005 91 )

#include <cctype>

#include "bot.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "bot_weapons.h"
#include "console_output.h"
#include "waypoint.h"


int gmsgTextMsg = 0;
int gmsgSayText = 0;
int gmsgShowMenu = 0;

// util functions prototypes
edict_t *UTIL_FindEntityByString( edict_t *pentStart, const char *szKeyword, const char *szValue );
void UTIL_BuildFileName(char *filename, char *arg1, char *arg2, bool separator);
void UTIL_MBLogPrint(char* fmt, ...);
bool IsStringValid(char *str);
void ProcessTheName(char *name);
void ShortenIt(char *name);
void DumpVector(FILE *f, Vector vec);


Vector UTIL_VecToAngles( const Vector &vec )
{
   float rgflVecOut[3];
   VEC_TO_ANGLES(vec, rgflVecOut);
   return Vector(rgflVecOut);
}


// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE) | (ignoreGlass?0x100:0), pentIgnore, ptr );

#ifdef DEBUG
	// SHOW ME THE LINES
	util.HighlightTrace(vecStart, vecEnd, pentIgnore);
#endif // DEBUG

}


void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), pentIgnore, ptr );

#ifdef DEBUG
	// SHOW ME THE LINES
	util.HighlightTrace(vecStart, vecEnd, pentIgnore);
#endif // DEBUG

}


/*
*/
void utils_t::InitPrivateVars(void)
{
	teamOne_player_count = 0;
	teamTwo_player_count = 0;
	teams_player_count_update_time = -1.0f;
}


/*
* calls standard TraceLine that doesn't ignore monsters, but will ignore any player entity it hits
*/
void utils_t::TraceLineIgnoringPlayers(const Vector& vecStart, const Vector& vecEnd, TraceResult* ptr)
{
	// just call standard trace line between the two points
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, NULL, ptr);

	int safety_stop = 0;

	// if we hit a player entity ...
	while (strcmp(STRING(ptr->pHit->v.classname), "player") == 0)
	{
		safety_stop++;
		if (safety_stop > 31)// there can be only 32 clients on the server, the chance to have them all in one line is basically zero :) ... still we'll use this value
			break;

		// then send a new traceline ignoring it this time
		UTIL_TraceLine(ptr->pHit->v.origin, vecEnd, dont_ignore_monsters, ptr->pHit, ptr);
	}

	/*/
#ifdef _DEBUG
	char msg[256];
	sprintf(msg, "repeated TraceLines count is #%d (classname of pHit is <%s> and Fraction is %.2f)\n",	safety_stop, STRING(ptr->pHit->v.classname), ptr->flFraction);
	conOutput.Notify(msg);
#endif
	/**/
}


void UTIL_MakeVectors( const Vector &vecAngles )
{
   MAKE_VECTORS( vecAngles );
}


edict_t *UTIL_FindEntityByString( edict_t *pentStart, const char *szKeyword, const char *szValue )
{
   edict_t *pentEntity;

   pentEntity = FIND_ENTITY_BY_STRING( pentStart, szKeyword, szValue );

   if (!FNullEnt(pentEntity))
      return pentEntity;
   return NULL;
}


// Defined in util.h
int UTIL_PointContents( const Vector &vec )
{
   return POINT_CONTENTS(vec);
}


// Defined in util.h	- NEVER USED
void UTIL_SetSize( entvars_t *pev, const Vector &vecMin, const Vector &vecMax )
{
   SET_SIZE( ENT(pev), vecMin, vecMax );
}


// Defined in util.h	- NEVER USED
void UTIL_SetOrigin( entvars_t *pev, const Vector &vecOrigin )
{
   SET_ORIGIN(ENT(pev), vecOrigin );
}


/*
* the max length of the whole message can be 127 characters plus the newline one if we want to keep formating, because of the size limit of the engine Text Message
* the terminating null character doesn't seem to be required, because it looks like the engine will always print 128 characters at max and then it stops the printing
*/
void ClientPrint(edict_t* pEntity, int msg_dest, const char* msg_name, const char* string)
{
	if (internals.IsOverrideClientPrint())
		return;

	char the_text[TEXT_MSG_SIZE];

	sprintf(the_text, "%s %s", string, msg_name);

	ClientPrint(pEntity, msg_dest, the_text);
}


/*
* the max length of the message can be 127 characters plus the newline one if we want to keep formating, because of the size limit of the engine Text Message
* the terminating null character doesn't seem to be required, because it looks like the engine will always print 128 characters at max and then it stops the printing
*/
void ClientPrint( edict_t *pEntity, int msg_dest, const char *msg_name)
{
	if (internals.IsOverrideClientPrint())
		return;

   if (gmsgTextMsg == 0)
      gmsgTextMsg = REG_USER_MSG( "TextMsg", -1 );

   pfnMessageBegin( MSG_ONE, gmsgTextMsg, NULL, pEntity );
   pfnWriteByte( msg_dest );
   pfnWriteString( msg_name );
   pfnMessageEnd();
}


/*		NEVER USED
void UTIL_SayText( const char *pText, edict_t *pEdict )
{
   if (gmsgSayText == 0)
      gmsgSayText = REG_USER_MSG( "SayText", -1 );

   pfnMessageBegin( MSG_ONE, gmsgSayText, NULL, pEdict );
   pfnWriteByte( ENTINDEX(pEdict) );
   pfnWriteString( pText );
   pfnMessageEnd();
}


void UTIL_HostSay( edict_t *pEntity, int teamonly, char *message )
{
   int   j;
   char  text[128];
   char *pc;
   int   sender_team, player_team;
   edict_t *client;

   // make sure the text has content
   for ( pc = message; pc != NULL && *pc != 0; pc++ )
   {
      if ( isprint( *pc ) && !isspace( *pc ) )
      {
         pc = NULL;   // we've found an alphanumeric character,  so text is valid
         break;
      }
   }

   if ( pc != NULL )
      return;  // no character found, so say nothing

   // turn on color set 2  (color on,  no sound)
   if ( teamonly )
      sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
   else
      sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

   j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
   if ( (int)strlen(message) > j )
      message[j] = 0;

   strcat( text, message );
   strcat( text, "\n" );

   // loop through all players
   // Start with the first player.
   // This may return the world in single player if the client types something between levels or during spawn
   // so check it, or it will infinite loop

   if (gmsgSayText == 0)
      gmsgSayText = REG_USER_MSG( "SayText", -1 );

   sender_team = UTIL_GetTeam(pEntity);

   client = NULL;
   while ( ((client = UTIL_FindEntityByClassname( client, "player" )) != NULL) &&
           (!FNullEnt(client)) ) 
   {
      if ( client == pEntity )  // skip sender of message
         continue;

      player_team = UTIL_GetTeam(client);

      if ( teamonly && (sender_team != player_team) )
         continue;

      pfnMessageBegin( MSG_ONE, gmsgSayText, NULL, client );
         pfnWriteByte( ENTINDEX(pEntity) );
         pfnWriteString( text );
      pfnMessageEnd();
   }

   // print to the sending client
   pfnMessageBegin( MSG_ONE, gmsgSayText, NULL, pEntity );
      pfnWriteByte( ENTINDEX(pEntity) );
      pfnWriteString( text );
   pfnMessageEnd();
   
   // echo to server console
   g_engfuncs.pfnServerPrint( text );
}
*/


#ifdef   DEBUG
edict_t *DBG_EntOfVars( const entvars_t *pev )
{
   if (pev->pContainingEntity != NULL)
      return pev->pContainingEntity;
   ALERT(at_console, "entvars_t pContainingEntity is NULL, calling into engine");
   edict_t* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);
   if (pent == NULL)
      ALERT(at_console, "DAMN!  Even the engine couldn't FindEntityByVars!");
   ((entvars_t *)pev)->pContainingEntity = pent;
   return pent;
}
#endif //DEBUG


edict_t* utils_t::FindEntityInSphere(edict_t* pentStart, const Vector& vecCenter, float flRadius)
{
	edict_t* pentEntity;

	pentEntity = FIND_ENTITY_IN_SPHERE(pentStart, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return pentEntity;

	return NULL;
}


// search for entity passed by its classname
edict_t* utils_t::FindEntityInSphere(edict_t* pCallerEdict, const char* ent_name)
{
	edict_t* pEntity = NULL;

	while ((pEntity = FindEntityInSphere(pEntity, pCallerEdict->v.origin, STANDARD_SEARCH_RADIUS)) != NULL)
	{
		if (strcmp(STRING(pEntity->v.classname), ent_name) == 0)
			return pEntity;
	}

	return NULL;
}


edict_t* utils_t::FindEntityByClassname(edict_t* pentStart, const char* szName)
{
	return UTIL_FindEntityByString(pentStart, "classname", szName);
}


edict_t* utils_t::FindEntityByTargetname(edict_t* pentStart, const char* szName)
{
	return UTIL_FindEntityByString(pentStart, "target", szName);
}


/*
* return the team based either on team variable or on the model "name" the Edict uses
*/
int utils_t::GetTeam(edict_t* pEntity)
{
	if (pEntity == NULL)
	{
#ifdef _DEBUG
		char msg[128]{};
		sprintf(msg, "<FATAL ERROR>(util.cpp|GetTeam()) pEntity is NULL\n");
		util.DebugDev(msg, -100, -100);
		conOutput.Notify(msg);
#endif

		return teamNULL;
	}

	// try the team variable first, this works in most of situations so it should be on top to save some CPU time
	if (pEntity->v.team == teamONE.GetTeamId())
		return teamONE.GetTeamId();
	if (pEntity->v.team == teamTWO.GetTeamId())
		return teamTWO.GetTeamId();

	// works for getting team info from thrown grenades in all supported versions
	if (pEntity->v.owner != NULL)
		return GetTeam(pEntity->v.owner);

	// try netname is the owner pointer failed, happens when the owner just left the game or something
	if (strcmp(STRING(pEntity->v.netname), teamONE.GetTeamNameNetname()) == 0)
		return teamONE.GetTeamId();
	else if (strcmp(STRING(pEntity->v.netname), teamTWO.GetTeamNameNetname()) == 0)
		return teamTWO.GetTeamId();

	/*/
#ifdef _DEBUG
	if (IsOldVersion() == FALSE)
		conOutput.Notify("***Can't get team from edict.team using edict.model\n");
#endif
	/**/

	// if previous checks failed then try to get the team info from the model (works in older FA versions)
	// Red team - sand(brown) camouflage
	if (strstr(STRING(pEntity->v.model), teamONE.GetTeamNameForPlayerModel()) != NULL)
		return teamONE.GetTeamId();
	// Blue team - jungle(green) camouflage
	else if (strstr(STRING(pEntity->v.model), teamTWO.GetTeamNameForPlayerModel()) != NULL)
		return teamTWO.GetTeamId();


#ifdef _DEBUG

	// don't print an error message in these situations
	// (ie. they are quite common and don't cause problems)
	//
	// 1)test if the client didn't get in game yet (currently picking a team, class etc.)
	//   FA doesn't seem to set spectator flag or this combination when someone spectates so this cannot handle players who do spectate
	// 2)the game world has no team so we must break it here happens when the bot get hurt after falling from something higher
	bool okay = TRUE;

	if (IsOldVersion())
	{
		// comment it out if needed
		//okay = FALSE;
	}

	if (!(pEntity->v.flags & (FL_CLIENT | FL_NOTARGET | FL_SPECTATOR)) && okay)
	{
		char msg[256]{};
		if ((pEntity->v.netname != NULL) && (pEntity->v.classname != NULL))
			sprintf(msg, "<BUG>Can't get team even from edict.model for edict <%s> (class <%s>)\n", STRING(pEntity->v.netname), STRING(pEntity->v.classname));
		else if (pEntity->v.classname != NULL)
			sprintf(msg, "<BUG>Can't get team even from edict.model for edict <no netname> (class <%s>)\n", STRING(pEntity->v.classname));
		else if (pEntity->v.netname != NULL)
			sprintf(msg, "<BUG>Can't get team even from edict.model for edict <%s> (class <no classname>)\n", STRING(pEntity->v.netname));
		else
			sprintf(msg, "<BUG>Can't get team even from edict.model for edict <no netname> (class <no classname>)\n");

		// don't dump grenades placed in the map as item (ie you can pick them up)
		if ((pEntity->v.solid == SOLID_TRIGGER) && (pEntity->v.movetype == MOVETYPE_TOSS))
			return teamNULL;

		conOutput.Notify(msg, true);
	}
#endif

	return teamNULL;  // return this flag if team is unknown
}

bool utils_t::AreTeammates(edict_t* pEntity1, edict_t* pEntity2) {
	if(pEntity1 == pEntity2)
		return true;

	if(!internals.IsTeamPlay())
		return false;

	auto team1 = GetTeam(pEntity1);
	auto team2 = GetTeam(pEntity2);
	if(team1 == teamNULL || team2 == teamNULL)
		return false;

	return team1 == team2;
}


/*
* returns the bot array index of passed edict
* returns -1 if the edict is NOT a bot
*/
int utils_t::GetBotIndex(edict_t *pEdict)
{
   int index;

   for (index=0; index < MAX_CLIENTS; index++)
   {
      if (bots[index].pEdict == pEdict)
      {
         return index;
      }
   }

   return -1;  // return -1 if edict is not a bot
}


/*		NEVER USED
bot_t *UTIL_GetBotPointer(edict_t *pEdict)
{
   int index;

   for (index=0; index < MAX_CLIENTS; index++)
   {
      if (bots[index].pEdict == pEdict)
      {
         break;
      }
   }

   if (index < MAX_CLIENTS)
      return (&bots[index]);

   return NULL;  // return NULL if edict is not a bot
}
*/


bool utils_t::IsAlive(edict_t *pEdict)
{
	return ((pEdict->v.deadflag == DEAD_NO) && !(pEdict->v.effects & EF_NODRAW) && (pEdict->v.health > 0) && !(pEdict->v.flags & FL_NOTARGET) && (pEdict->v.solid == SOLID_SLIDEBOX));
}


bool utils_t::IsEdictProne(edict_t* pEdict)
{
	return ((pEdict->v.iuser3 == USR3_PRONE) || (pEdict->v.iuser3 == USR3_BIPOD_USED));
}


bool utils_t::IsEdictCrouched(edict_t* pEdict)
{
	return ((pEdict->v.flags & FL_DUCKING) && (pEdict->v.iuser3 != USR3_PRONE) && (pEdict->v.iuser3 != USR3_BIPOD_USED));
}


/*
* returns TRUE if entity classname matches given name
*/
bool utils_t::IsEntityName(edict_t* pEntity, const char* entity_classname)
{
	return (strcmp(entity_classname, STRING(pEntity->v.classname)) == 0);
}


/*
* returns TRUE if this entity classname matches one of the door types
*/
bool utils_t::IsDoorEntity(edict_t* pEntity)
{
	return (IsEntityName(pEntity, "func_door") || IsEntityName(pEntity, "func_door_rotating") || IsEntityName(pEntity, "momentary_door"));
}


/*
* returns TRUE is the door is open
* specific door spawn flags are defined in 'door.h' in HLSDK, but the most useful ones are: 1 = start open, 16 = one way, 32 = door won't auto close, 256 = press 'use' key to open
*/
bool utils_t::IsDoorOpen(edict_t* pEntity, bool is_crouched)
{
	// is it sliding door OR wheel operated door (eg. warlord's safe on obj_thanatos)?
	if (IsEntityName(pEntity, "func_door") || IsEntityName(pEntity, "momentary_door"))
	{
		// then check on which axis the door moves and if it's open enough to make the size of the body fit in (player body size is 32 on 32 and 72 tall in standing stance)
		if (((fabs(pEntity->v.movedir.x) == 1.0f) && (fabs(pEntity->v.origin.x) > (pEntity->v.size.x * 0.66f)) && (fabs(pEntity->v.origin.x) > 32.0f)) ||
			((fabs(pEntity->v.movedir.y) == 1.0f) && (fabs(pEntity->v.origin.y) > (pEntity->v.size.y * 0.66f)) && (fabs(pEntity->v.origin.y) > 32.0f)))
			return true;

		if ((fabs(pEntity->v.movedir.z) == 1.0f) && ((is_crouched && (fabs(pEntity->v.origin.z) > 36.0f)) || (fabs(pEntity->v.origin.z) > 72.0f)))
			return true;
	}

	// is it swing door?
	if (IsEntityName(pEntity, "func_door_rotating"))
	{
		// first check which axis the door swing on and then appropriate angles tell how open the door is at the moment (typically 90 degree mean fully open)
		if (((fabs(pEntity->v.movedir.x) == 1.0f) && (fabs(pEntity->v.angles.x) >= 75.0f)) || ((fabs(pEntity->v.movedir.y) == 1.0f) && (fabs(pEntity->v.angles.y) >= 75.0f)) ||
			((fabs(pEntity->v.movedir.z) == 1.0f) && (fabs(pEntity->v.angles.z) >= 75.0f)))
			return true;
	}

	return false;
}


/*
* checks if the file exists
*/
bool UTIL_IsFile(const char* filename)
{
	FILE* fp;

	fp = fopen(filename, "r");

	if (fp != NULL)
	{
		fclose(fp);
		return true;
	}

	return false;
}


/*
* returns true if the waypointer is a member of MB team
*/
bool utils_t::IsOfficialWaypoints(const char* waypointer)
{
	if (strstr(waypointer, "Frank McNeil"))
		return true;

	return false;
}


/*
* checks the mod version to see if it's one of new versions ie. versions that include a lot of differences that are imcompatible with previous version
*/
bool utils_t::IsNewerVersion(void)
{
	return false;
}


/*
* checks the mod version to see if it's one of pre 2.6 versions ie. versions that don't support edict->v.team and gl attachments are fired right when you press secondary fire,
* you can't sprint, you can't merge magazines etc.
*/
bool utils_t::IsOldVersion(void)
{
	return false;
}


bool utils_t::IsInViewCone(Vector *pOrigin, edict_t *pEdict)
{
	Vector2D vec2LOS;
	float    flDot;
	
	UTIL_MakeVectors ( pEdict->v.angles );
	
	vec2LOS = ( *pOrigin - pEdict->v.origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();
	
	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );
	
	if ( flDot > 0.50f )  // 60 degree field of view
	{
		return true;
	}
	else
	{
		return false;
	}
}


// for more accurate checks
bool utils_t::IsInNarrowViewCone(Vector *pOrigin, edict_t *pEdict, float cone)
{
	Vector2D vec2LOS;
	float    flDot;
	
	UTIL_MakeVectors ( pEdict->v.angles );
	
	vec2LOS = ( *pOrigin - pEdict->v.origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();
	
	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );




	//@@@@@@@@@@@@@@@@@@@@@@@
	/**/
#ifdef _DEBUG
	if (botdebugger.IsDebugAims())
	{
		char m[256]{};
		static char last_m[256]{};

		sprintf(m, "(util.cpp) NarrowViewCone() - dot result %.2f\n", flDot);

		if (strcmp(m, last_m) != 0)
		{
			conOutput.Notify(m);

			strcpy(last_m, m);
		}
	}
#endif
	/**/




	
	if ( flDot >= cone )  // by default it is something like 35 and less degree field of view
	{
		return true;
	}
	else
	{
		return false;
	}
}


/*
* works very similar to IsInViewCone(), but it returns exact angle value instead of TRUE or FALSE
*/
int utils_t::InFieldOfView(edict_t* pEdict, const Vector& dest)
{
	// find angles from source to destination...
	Vector entity_angles = UTIL_VecToAngles(dest);

	// make yaw angle 0 to 360 degrees if negative...
	if (entity_angles.y < 0)
		entity_angles.y += 360;

	// get current view angle...
	float view_angle = pEdict->v.v_angle.y;

	// make view angle 0 to 360 degrees if negative...
	if (view_angle < 0)
		view_angle += 360;

	// return the absolute value of angle to destination entity
	// zero degrees means straight ahead,  45 degrees to the left or
	// 45 degrees to the right is the limit of the normal view angle

	// rsm - START angle bug fix
	int angle = abs((int)view_angle - (int)entity_angles.y);

	if (angle > 180)
		angle = 360 - angle;

	return angle;
	// rsm - END
}


bool utils_t::IsVisible(const Vector& vecOrigin, edict_t* pEdict)
{
	TraceResult tr;
	Vector      vecLookerOrigin;

	// look through caller's eyes
	vecLookerOrigin = pEdict->v.origin + pEdict->v.view_ofs;

	int bInWater = (UTIL_PointContents(vecOrigin) == CONTENTS_WATER);
	int bLookerInWater = (UTIL_PointContents(vecLookerOrigin) == CONTENTS_WATER);

	// don't look through water
	if (bInWater != bLookerInWater)
		return false;

	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, pEdict, &tr);

	if (tr.flFraction != 1.0f)
	{
		return false;  // Line of sight is not established
	}
	else
	{
		return true;  // line of sight is valid.
	}
}


// ehm "overloaded" to ignore water restriction
bool utils_t::IsVisible(const Vector &vecOrigin, edict_t *pEdict, bool ignore_water_restriction)
{
	TraceResult tr;
	Vector      vecLookerOrigin;
	
	// look through caller's eyes
	vecLookerOrigin = pEdict->v.origin + pEdict->v.view_ofs;
	
	int bInWater = (UTIL_PointContents (vecOrigin) == CONTENTS_WATER);
	int bLookerInWater = (UTIL_PointContents (vecLookerOrigin) == CONTENTS_WATER);
	
	// don't look through water 
	if ((bInWater != bLookerInWater) && (ignore_water_restriction == false))
		return false;
	
	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, pEdict, &tr);
	
	if (tr.flFraction != 1.0f)
	{
		return false;  // Line of sight is not established
	}
	else
	{
		return true;  // line of sight is valid.
	}
}


/*
* tests direct visibility between the bot and the target including tests for teammate, fence etc.
*/
int utils_t::IsPlayerVisible(const Vector &vecOrigin, const Vector &vecLookerOrigin, edict_t *pEdict)
{
	TraceResult tr;

	int bInWater = (UTIL_PointContents(vecOrigin) == CONTENTS_WATER);
	int bLookerInWater = (UTIL_PointContents(vecLookerOrigin) == CONTENTS_WATER);

	// don't look through water
	if (bInWater != bLookerInWater)
		return VIS_WATER;
		//return VIS_NO;

	// trace to see if there is a direct visibility at the enemy
	// we can't ignore monters, because if we did we would not see teammates and we would do team kills all the time
	UTIL_TraceLine(vecLookerOrigin, vecOrigin, dont_ignore_monsters, dont_ignore_glass, pEdict, &tr);


	//@@@@@@@@@@@@@
	/*/
#ifdef _DEBUG
	char strs[128]{};
	sprintf(strs, "(PlayerVisible - FIRST test)Result=%.2f | entity=%s(netname=%s)(target=%s)(targetname=%s)\n",
		tr.flFraction, STRING(tr.pHit->v.classname), STRING(tr.pHit->v.netname), STRING(tr.pHit->v.target), STRING(tr.pHit->v.targetname));
	conOutput.Notify(strs);
#endif
	/**/

	// do we have clear view?
	// when we don't ignore monsters then this won't happen offen, because the player entity has a size
	// and if the bot and his enemy are face to face then enemy origin is "hidden behind" the player entity
	if (tr.flFraction == 1.0)
		return VIS_YES;

	// we don't have another entity in the way (i.e. it's a solid world that blocks the direct visibility) therefore we don't need any other tests
	if (IsEntityName(tr.pHit, "worldspawn"))
		return VIS_NO;

	// first backup the entity that blocks the direct visibility for later use
	edict_t *obstacle = tr.pHit;

	// now we have to ignore the previous obstacle in order to see if the trace line can finally reach the target
	// i.e. virtually remove the obstacle and check direct visibility
	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, dont_ignore_glass, obstacle, &tr);


	//@@@@@@@@@@@@@
	/*/
#ifdef _DEBUG
	char str[256]{};
	sprintf(str, "(PlayerVisible - 2nd test)Result=%.2f | entity=%s(netname=%s)(target=%s)(targetname=%s)\n",
		tr.flFraction, STRING(tr.pHit->v.classname), STRING(tr.pHit->v.netname), STRING(tr.pHit->v.target), STRING(tr.pHit->v.targetname));
	conOutput.Notify(str);
#endif
	/**/

	// if we can hit worldspawn now i.e. flFraction == 1.0 && pHit->v.classname is worldspawn then we can establish a line of sight through the entity we ignored in the 2nd traceline
	// so now just decide if we can really see through the ignored entity and if we can open fire (i.e. don't do a TK)

	// the traceline reached the enemy without hitting anything else -> bot may eventually open fire
	if (IsEntityName(tr.pHit, "worldspawn") && (tr.flFraction == 1.0))
	{
		/*/
#ifdef DEBUG
		char str[256]{};
		sprintf(str, "(%s)*** I see <%s>!\n", STRING(pEdict->v.netname), STRING(obstacle->v.classname));
		conOutput.Notify(str);
#endif // DEBUG
		/**/


		// there was a player entity so we need to check if it isn't our teammate (i.e. teammate standing between the bot and the enemy)
		if (IsEntityName(obstacle, "player"))
		{
			// we would hit a teammate so don't shoot and risk a team kill
			if (AreTeammates(pEdict, obstacle))
			{

				//@@@@@
#ifdef DEBUG
				//conOutput.Notify("*** BREAKING MY TEAMMATE IS IN THE WAY!!!\n");
#endif // DEBUG

				return VIS_NO;
			}

			// it must be enemy so the bot can open fire
			return VIS_YES;
		}

		// there was a breakable entity, all we need to do is check if the entity is done in a way players can see through (ie. some transparency and not rendered as normal or solid)
		if (IsEntityName(obstacle, "func_breakable") &&	(obstacle->v.rendermode != kRenderNormal) && (obstacle->v.rendermode != kRenderTransAlpha) &&
			(obstacle->v.renderamt <= 150))	// NEEDS more tests								!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		{

			//@@@@@
#ifdef DEBUG
			//conOutput.Notify("*** I see through func breakable!\n");
#endif // DEBUG


			return VIS_YES;
		}

		// there was an entity that is meant to be part of the world, but it's done so that you can see through it, but you can't pass through it of course ... a fence for example
		if (IsEntityName(obstacle, "func_wall"))
		{

			//@@@@@
			/*/
#ifdef DEBUG
			char str[256]{};
			sprintf(str, "(%s)*** I see func wall!\n", STRING(pEdict->v.netname));
			conOutput.Notify(str);
#endif // DEBUG
			/**/

			/*/
			// this is ugly solution, but the central alley causes real problems
			// as long as there isn't better way how to deal with it we have to use texture names to figure out if it's a see through object or not
			if (strcmp(STRING(gpGlobals->mapname), "obj_armory") == 0)
			{
				const char *texture = NULL;
				texture = g_engfuncs.pfnTraceTexture(obstacle, vecLookerOrigin, vecOrigin);

				if ((texture != NULL) && ((strcmp(texture, "{nm_leaves3") == 0) || (strcmp(texture, "{artrellis") == 0)))
				{

#ifdef DEBUG
					//conOutput.Notify("*** I see enemy! (func wall -> fence/bush on obj_armory)\n");
#endif // DEBUG


					return VIS_FENCE;
				}

				return VIS_NO;
			}
			/**/

			// a transparent alpha texture case (ie. a fence, barbed wire etc.)
			if ((obstacle->v.rendermode == kRenderTransAlpha) && (obstacle->v.renderamt == 255) && (obstacle->v.flags & FL_WORLDBRUSH))
			{
				//@@@@@
#ifdef DEBUG
				//conOutput.Notify("*** I see enemy! (func wall -> fence/barbed wire)\n");
#endif // DEBUG
				return VIS_FENCE;
			}
			// a transparent texture case (ie. a window, glass wall etc.)
			else if ((obstacle->v.rendermode == kRenderTransTexture) && (obstacle->v.renderamt <= 150) && (obstacle->v.flags & FL_WORLDBRUSH))
			{
				//@@@@@
#ifdef DEBUG
				//conOutput.Notify("*** I see enemy! (func wall -> window/glass)\n");
#endif // DEBUG
				return VIS_YES;
			}
		}
	}

	// line of sight is not established (the enemy must be behind something like a truck or tank and like models), basically something that we cannot and should not see through
	return VIS_NO;
}


// same as above, but we use caller's eyes by default
int utils_t::IsPlayerVisible( const Vector &vecOrigin, edict_t *pEdict )
{
	Vector vecLookerOrigin;
	
	// look through caller's eyes
	vecLookerOrigin = pEdict->v.origin + pEdict->v.view_ofs;

	return IsPlayerVisible(vecOrigin, vecLookerOrigin, pEdict);
}


/*
* returns TRUE if given waypoint better to say its origin is in field of view AND is also directly visible either the origin itself or the spot at the top or the bottom of the waypoint
* so that the waypoint can be displayed even if it is behind a sandbags or barbed wire etc. where the Traceline would hit this obstacle if we were checking just the origin
*/
bool utils_t::IsWaypointVisible(Vector& vecOrigin, edict_t* pEdict)
{
	return (IsInViewCone(&vecOrigin, pEdict) && (IsVisible(vecOrigin, pEdict, true) || IsVisible(vecOrigin + Vector(0, 0, 34), pEdict, true) || IsVisible(vecOrigin - Vector(0, 0, 34), pEdict, true)));
}


/*
* returns TRUE if both origins exist and there is a known door entity between them
* this is used to allow displaying a path beam between two neighbouring waypoints where one is in front of the door and is visible, but the other one lies on the other side and isn't currently visible
*/
bool utils_t::IsWaypointVisibleThroughDoor(Vector& vecOrigin, Vector other_vecOrigin, edict_t* pEdict)
{
	if ((other_vecOrigin != g_vecZero) && IsWaypointVisible(other_vecOrigin, pEdict))
	{
		TraceResult tr;

		// send a Traceline between the two waypoints
		UTIL_TraceLine(other_vecOrigin, vecOrigin, ignore_monsters, ignore_glass, pEdict, &tr);

		if (tr.flFraction != 1.0f)
		{
			// did it hit door entity?
			if (IsDoorEntity(tr.pHit))
			{
				// then try it again ... this time ignoring the door entity we hit before
				UTIL_TraceLine(other_vecOrigin, vecOrigin, ignore_monsters, ignore_glass, tr.pHit, &tr);

				if (tr.flFraction == 1.0f)
					return true;
			}
		}
		else
			return true;
	}

	return false;
}


Vector utils_t::GetGunPosition(edict_t *pEdict)
{
   return (pEdict->v.origin + pEdict->v.view_ofs);
}


Vector utils_t::VecBModelOrigin(edict_t *pEdict)
{
   return pEdict->v.absmin + (pEdict->v.size * 0.5);
}


Vector utils_t::VecAbsoluteOrigin(edict_t* pEdict)
{
	return (pEdict->v.absmax + pEdict->v.absmin) * 0.5f;
}


/*
* returns the origin for both the standard entities as well as the BModels with zero origin
*/
Vector utils_t::GetEntityOrigin(edict_t* pEdict)
{
	if (pEdict->v.origin == g_vecZero)
		return VecBModelOrigin(pEdict);

	return pEdict->v.origin;
}


/*
* builds virtual path inside Half-Life
* separator == TRUE means that we need an additional directory seperator at the end of the path (eg. Half-Life\firearms\)
* separator == FALSE means no dir. separator will be added in the end of the path (eg. Half-Life\firearms)
*/
void UTIL_BuildFileName(char *filename, char *arg1, char *arg2, bool separator)
{
	if ((arg1) && (*arg1))
		strcpy(filename, arg1);

	if ((arg2) && (*arg2))
	{
#ifndef __linux__
		strcat(filename, "\\");
#else
		strcat(filename, "/");
#endif

		strcat(filename, arg2);
	}
	
	if (separator)
	{
#ifndef __linux__
		strcat(filename, "\\");
#else
		strcat(filename, "/");
#endif
	}


#ifdef _DEBUG
	extern int debug_engine;

	if (debug_engine)
	{
		FILE *fp;
		fp=fopen("!mb_engine_debug.txt","a");
		fprintf(fp, "\nVirtual path build by BuildFileName(): \"%s\"\n", filename);
		fclose(fp);
	}
#endif
}


/*
* builds virtual path inside Half-Life
* filename will be arg1\arg2\arg3 on Windows or arg1/arg2/arg3 on Linux
*/
void UTIL_BuildFileName(char* filename, char* arg1, char* arg2, char* arg3)
{
	if ((arg1) && (*arg1))
		strcpy(filename, arg1);

	if ((arg2) && (*arg2))
	{
#ifndef __linux__
		strcat(filename, "\\");
#else
		strcat(filename, "/");
#endif

		strcat(filename, arg2);
	}

	if ((arg3) && (*arg3))
	{
#ifndef __linux__
		strcat(filename, "\\");
#else
		strcat(filename, "/");
#endif

		strcat(filename, arg3);
	}


#ifdef _DEBUG
	extern int debug_engine;

	if (debug_engine)
	{
		FILE* fp;
		fp = fopen("!mb_engine_debug.txt", "a");
		fprintf(fp, "\nVirtual path build by BuildFileName(): \"%s\"\n", filename);
		fclose(fp);
	}
#endif
}


void UTIL_StringFromBuffer(char* string, const char* buffer, int left_bracket_char, int right_bracket_char)
{
	int pos1 = 0;				// position in buffer
	int pos2 = 0;				// position in string

	int length = strlen(buffer);	// get buffer lenght

	// negative value is invalid so we will ignore the left bracket character
	// and we'll start at the beginning of the buffer
	if (left_bracket_char < 0)
		;
	else
	{
		// go through the buffer till the opening bracket character
		while ((buffer[pos1] != left_bracket_char) && (pos1 < length))
			pos1++;

		// move past the opening bracket
		pos1++;

		// we didn't find the opening bracket
		if (pos1 == length)
			return;
	}

	string[0] = 0;

	// now read the buffer character by character and copy them to the string
	while ((buffer[pos1] != right_bracket_char) && (pos1 < length))
	{
		string[pos2] = buffer[pos1];

		pos1++;
		pos2++;
	}

	// terminate the string properly
	string[pos2] = 0;

	return;
}


//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( char *fmt, ... )
{
   va_list        argptr;
   static char    string[1024];
   
   va_start ( argptr, fmt );
   vsprintf ( string, fmt, argptr );
   va_end   ( argptr );

   // Print to server console
   ALERT( at_logged, "%s", string );
}


/*
* works same as UTIL_LogPrintf only adds [MARINE_BOT] header before the logged message
*/
void UTIL_MBLogPrint(char *fmt, ...)
{
	va_list        argptr;
	static char    string[1024];
	
	va_start ( argptr, fmt );
	vsprintf ( string, fmt, argptr );
	va_end   ( argptr );
	
	// Print to server console with MB header
	ALERT( at_logged, "[MARINE_BOT] %s", string );
}


/*
* builds virtual path inside MB folder (by default it's "marine_bot")
*/
void utils_t::MarineBotFileName(char* filename, const char* arg1, const char* arg2)
{
	// build initial point first (ie. point to MB's folder)
	// we are using char variable that holds mod directory name to allow using MB also in non-standard mod installations (eg. "Half-Life/FA" instead of default "Half-Life/firearms")

	// we must know mod folder name else we can't start to build any path inside it
	if (mod_dir_name[0])
		UTIL_BuildFileName(filename, mod_dir_name, internals.GetMBFolderName(), true);
	else
	{
		filename[0] = 0;
		return;
	}

	// then add additional parameters such as subdirectory name etc.
	if ((arg1) && (*arg1) && (arg2) && (*arg2))
	{
		strcat(filename, arg1);

#ifndef __linux__
		strcat(filename, "\\");
#else
		strcat(filename, "/");
#endif

		strcat(filename, arg2);
	}
	else if ((arg1) && (*arg1))
	{
		strcat(filename, arg1);
	}
}


/*
//		NOT USED & WILL PROBABLY NEVER BE USED
// trace tree lines forward - standing position eyes, duck position, prone position
int ForwardTrace(const Vector &vecOrigin, edict_t *pEdict)
{
	TraceResult tr;
	Vector      vecLookerOrigin;
	Vector		fake_origin;
	bool		duck_trace_failed;

	fake_origin = pEdict->v.origin;
	duck_trace_failed = FALSE;

	vecLookerOrigin = pEdict->v.origin;

	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, pEdict, &tr);

	if (tr.flFraction != 1.0)
		duck_trace_failed = TRUE;

	// look through caller's eyes
	vecLookerOrigin = pEdict->v.origin + pEdict->v.view_ofs;

	int bInWater = (UTIL_PointContents (vecOrigin) == CONTENTS_WATER);
	int bLookerInWater = (UTIL_PointContents (vecLookerOrigin) == CONTENTS_WATER);

	// don't look through water
	if (bInWater != bLookerInWater)
		return -1;

	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, pEdict, &tr);

	if (tr.flFraction != 1.0)
	{
		return 0;  // Line of sight is not established
	}
	else
	{
		if (duck_trace_failed == FALSE)
			return 2;	// line from eyes & duck is valid
		else
			return 1;	// only line from eyes is valid.
	}
}
*/


/*
* checks if there is a free space (for a body) on specified side of the object passed in by an origin
*/
bool utils_t::TraceObjectsSides(edict_t *pEdict, int side, const Vector &obj_origin)
{
	//@@@@@@@@@@@@@@@@@@@@@@@@@@
	// NO CODE YET


	return false;
}


/*
* use hand signal specified via command
*/
void utils_t::HandSignal(edict_t* pEdict, const char* signal)
{
	if ((pEdict == NULL) || (signal == NULL))
		return;

	FakeClientCommand(pEdict, signal, NULL, NULL);	// the signal
}


/*
* use DoD voice command specified via command argument
*/
void utils_t::Voice(edict_t *pEdict, voiceCmd command)
{
	if (pEdict == NULL)
		return;

	const char* vm2 = { "voice_menu2" };
	const char* vm3 = { "voice_menu3" };
	char menu[12]{};
	char slot[2]{};

	if (command == voiceCmd::area_clear)
	{
		// define in which menu and on what slot is this voice message located
		strcpy(menu, vm2);
		strcpy(slot, "9");
	}
	else if (command == voiceCmd::yes_sir)
	{
		strcpy(menu, vm2);
		strcpy(slot, "1");
	}
	else if (command == voiceCmd::negative)
	{
		strcpy(menu, vm2);
		strcpy(slot, "2");
	}
	else if (command == voiceCmd::coverme)
	{
		strcpy(menu, vm2);
		strcpy(slot, "3");
	}
	else if (command == voiceCmd::fire_in_the_hole)
	{
		strcpy(menu, vm2);
		strcpy(slot, "4");
	}
	else if (command == voiceCmd::grenade)
	{
		strcpy(menu, vm2);
		strcpy(slot, "5");
	}
	else if (command == voiceCmd::enemy_ahead)			// doesn't seem to work correctly - the hand signaling randomly points to wrong directions
	{
		strcpy(menu, vm3);
		strcpy(slot, "3");
	}

	// is there valid message?
	if (menu[0] != 0)
	{
		FakeClientCommand(pEdict, menu, NULL, NULL);	// activate the voice menu
		FakeClientCommand(pEdict, "menuselect", slot, NULL);	// select the message
	}

#ifdef DEBUG
	else
	{
		DebugInFile("utilVoice() -> undefined command used");
	}
#endif // DEBUG

}


/*
* use say command to send the message
*/
/*/																		NOT USED
void utils_t::Say(edict_t *pEdict, const char *message)
{
	if ((pEdict != NULL) && (message != NULL))
		FakeClientCommand(pEdict, "say", message, NULL);
}
/**/


/*
* use team say command to send the message
*/
void utils_t::TeamSay(edict_t *pEdict, const char *message)
{
	if ((pEdict != NULL) && (message != NULL))
		FakeClientCommand(pEdict, "say_team", message, NULL);
}


/*
* returns true if the bot is close enough to hear that call also checks the team of the invoker (ie don't react on enemies)
*/
bool utils_t::CanBotHearThisVoiceMessage(bot_t* pBot, edict_t* pInvoker, float range)
{
	edict_t* pEdict = pBot->pEdict;

	// first check whether the invoker of the voice command is visible at all
	if (IsVisible(pInvoker->v.origin + pInvoker->v.view_ofs, pEdict) == false)
		return false;

	// teams don't match so break it
	if (!AreTeammates(pInvoker, pEdict))
		return false;

	// get the distance to the invoker
	float distance = (pEdict->v.origin - pInvoker->v.origin).Length();

	// set the ability of the bot to hear the call, based on bot skill level (ie better bots can hear sounds from bigger distance)
	float sensitivity = range - (pBot->GetBotSkill() * (range / 10.0f));

	// is the bot close enough to hear it
	if (distance < sensitivity)
		return true;

	return false;
}


/*
* returns true if the bot can see the hand signal, checking position and team of the invoker
*/
bool utils_t::CanBotSeeThisHandSignal(bot_t* pBot, edict_t* pInvoker, float range)
{
	edict_t* pEdict = pBot->pEdict;

	// first check whether the invoker of the voice command is visible at all
	if (IsVisible(pInvoker->v.origin + pInvoker->v.view_ofs, pEdict) == false)
		return false;

	// teams doesn't match so break it
	if (!AreTeammates(pInvoker, pEdict))
		return false;

	// get the distance to the invoker
	float distance = (pEdict->v.origin - pInvoker->v.origin).Length();

	// is the bot close enough to see the signal at all
	if (distance > range)
		return false;

	// is the hand signal invoker within bot's field of view ie. not somewhere behind bot
	if (IsInViewCone(&pInvoker->v.origin, pEdict) == false)
		return false;

	// finally we will generate a chance to make the bot act like he didn't notice it, ie. the worse the bot skill is the higher chance to not notice the signal
	if (RANDOM_LONG(1, 100) < (95 - (5 * pBot->GetBotSkill())))
		return true;

	return false;
}


/*
* returns index of the bot matching the name passed by the agrument
*/
int utils_t::FindBotByName(const char *name_string)
{
	if ((name_string != NULL) && (*name_string != 0))
	{
		for (int index = 0; index < MAX_CLIENTS; index++)
		{
			if (bots[index].is_used == false)
				continue;

			// matches this bot name (from edict struct) with the one we are looking for?
			if ((strstr(STRING(bots[index].pEdict->v.netname), name_string)) != NULL)
			{
				return index;
			}
		}
	}

	return -1;
}


/*
* kicks random bot in specified team or all bots on server
*/
bool utils_t::KickBot(int which_one)
{
	// specific bot
	if ((which_one >= 0) && (which_one < MAX_CLIENTS))
	{
		if (bots[which_one].is_used)  // is this slot used?
		{
			char cmd[80]{};
			
			sprintf(cmd, "kick \"%s\"\n", bots[which_one].name);
			
			SERVER_COMMAND(cmd);  // kick the bot using (kick "name")

			return true;
		}
	}

	// all
	if (which_one == 100)
	{
		int temp = 0;

		for (int i=0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				temp++;

				char cmd[80]{};

				sprintf(cmd, "kick \"%s\"\n", bots[i].name);

				SERVER_COMMAND(cmd);
			}
		}

		if (temp > 0)
			return true;
	}

	// random team one or random team two
	if ((which_one == 100 + teamONE.GetTeamId()) || (which_one == 100 + teamTWO.GetTeamId()))
	{
		int i, j, this_one;
		int adequate[MAX_CLIENTS]{};

		j = 0;

		// set back current team number (only 1 or 2 are valid team numbers)
		which_one -= 100;

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].GetBotTeam() == which_one) && (bots[i].is_used))
			{
				adequate[j] = i;
				j++;
			}
		}

		if (j == 0)
			return false;

		this_one = RANDOM_LONG(1, j) - 1;

		char cmd[80]{};

		sprintf(cmd, "kick \"%s\"\n", bots[adequate[this_one]].name);

		SERVER_COMMAND(cmd);

		return true;
	}

	// random bot from any team
	if (which_one == -100)
	{
		int i, in_game;
		int adequate[MAX_CLIENTS]{};
		
		i = in_game = 0;

		// count how many bots is in game
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				adequate[in_game] = i;
				in_game++;
			}
		}

		if (in_game < 1)
			return false;

		i = RANDOM_LONG(1, in_game) - 1;

		char cmd[80]{};

		sprintf(cmd, "kick \"%s\"\n", bots[adequate[i]].name);

		SERVER_COMMAND(cmd);

		return true;

	}

	return false;
}


/*
* works same as kickbot only kills them
*/
bool utils_t::KillBot(int which_one)
{
	int temp = 0;

	// specific bot
	if ((which_one >= 0) && (which_one < MAX_CLIENTS))
	{
		if (bots[which_one].is_used)
		{
			ClientKill(bots[which_one].pEdict);

			return true;
		}
	}

	// all
	if (which_one == 100)
	{
		for (int i=0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				temp++;
				ClientKill(bots[i].pEdict);
			}
		}

		if (temp > 0)
			return true;
	}

	// random team one or random team two
	if ((which_one == 100 + teamONE.GetTeamId()) || (which_one == 100 + teamTWO.GetTeamId()))
	{
		which_one -= 100;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].GetBotTeam() != which_one)
				continue;

			if (bots[i].is_used)
			{
				temp++;
				ClientKill(bots[i].pEdict);
			}
		}

		if (temp > 0)
			return true;
	}

	return false;
}


/*
* counts all real/human clients from given team and returns this number
*/
int utils_t::CountPlayers(int team)
{
	int i, num=0;

	for (i = 0; i < MAX_CLIENTS; ++i)
	{
		// skip non existent players
		if (clients[i].pEntity == NULL)
		{
			continue;
		}

		// skip all bots
		if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
		{
			continue;
		}

		if (GetTeam(clients[i].pEntity) == team)
		{
			++num;
		}
	}

	return num;
}


/*
*/
int utils_t::GetTeamOnePlayerCount(void)
{
	if (teams_player_count_update_time < gpGlobals->time)
	{
		CountPlayersInBothTeams();
		teams_player_count_update_time = gpGlobals->time;
	}

	return teamOne_player_count;
}


/*
*
*/
int utils_t::GetTeamTwoPlayerCount(void)
{
	if (teams_player_count_update_time < gpGlobals->time)
	{
		CountPlayersInBothTeams();
		teams_player_count_update_time = gpGlobals->time;
	}

	return teamTwo_player_count;
}


/*
* SECTION rewrote & fixed by kota@
* counts all clients on server and returns difference based on team that should be lowered
* red team diff is returned with value + 100
*/
int utils_t::TeamsBalanceCheck(void)
{
	int i, actual_pl, bot_count, reds, blues, diff;

	actual_pl = bot_count = reds = blues = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].pEntity == NULL)
			continue;
		else
		{
			++actual_pl;

			if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
				++bot_count;

			// this is better way then just test v.team, because v.team doesn't always work
			if (GetTeam(clients[i].pEntity) == teamONE.GetTeamId())
				reds++;
			if (GetTeam(clients[i].pEntity) == teamTWO.GetTeamId())
				blues++;
		}
	}

	// are there NO clients (i.e. server is empty)
	if (actual_pl == 0)
		return -2;
	// there are no bots so can't do the balance
	else if (bot_count == 0)
		return -1;
	// is there odd number of actual players
	// get the difference
	diff = reds - blues;

//#ifdef _DEBUG
//	ALERT(at_console, "<DEV DEBUG> Balance Teams() - clients %d | bots %d | reds %d | blues %d | diff %d\n",
//		actual_pl, bot_count, reds, blues, diff);
//#endif

	// nothing to do, teams already balanced as possible
	if (diff > -2 && diff < 2)
	{
		return 0;
	}
	// we need to decrease red team
	else if (diff > 0)
	{
		return diff/2 + 100;
	}

	// we need to increase red team because diff<0
	return diff/(-2);
}


/*
* gets team difference from bot_manager and evens teams if needed
*/
int utils_t::ExecuteTeamsBalance(void)
{
	extern bool is_dedicated_server;
 
	// if teams are already balanced
	if (botmanager.GetTeamsBalanceValue() <= 0)
	{
		util.DebugInFile("<<BUG>> Invalid call of Execute Teams Balance() -> teams balance value <= 0\n");
		
		return -128;
	}

	static int state = 0;		// current state of state machine
	static int bots_to_go = -1;
	static int team = teamNULL;
	static int bot_index = NO_VAL;

	// do the first run init
	switch (state)
	{
		case 0:
			{
				if (botmanager.GetTeamsBalanceValue() > 100)
				{
					team = teamONE.GetTeamId();
					int temp = botmanager.GetTeamsBalanceValue();
					temp -= 100;
					bots_to_go = temp;
				}
				else
				{
					team = teamTWO.GetTeamId();
					bots_to_go = botmanager.GetTeamsBalanceValue();
				}

				state++;
			}

		case 1:
			{
				bot_index = NO_VAL;

				for (int index = 31; index >= 0; index--)
				{
					// skip unused slots
					if (bots[index].is_used == false)
						continue;

					// stop when have right team
					if (bots[index].GetBotTeam() == team)
					{
						bot_index = index;
						break;
					}
				}

				// only if is valid index (due to this: static int bot_index = -1;)
				//fatality, break balance!!! kota@
				if (bot_index < 0)
				{
					bots_to_go = 0;
					break;
				}

				char cmd[80]{};
				sprintf(cmd, "kick \"%s\"\n", bots[bot_index].name);
				SERVER_COMMAND(cmd);  // kick the bot using (kick "name")

				state++;

				break;
			}

		case 2:
			{				
				if (team == teamONE.GetTeamId())
				{
					// team number must be swapped here, because if previous bot was in one team then the new bot has to choose the other team in order even out the counts
					BotCreate(NULL, teamTWO.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
				}
				else if (team == teamTWO.GetTeamId())
				{
					BotCreate(NULL, teamONE.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
				}
				
				state++;
				
				break;
			}
		case 3:
			{
				bots_to_go--;
				// reset these before next round
				state = 1;		// must be reset to 1 becase state 0 is full init
				bot_index = NO_VAL;
			}
	}
	
	// are teams balanced now
	if (bots_to_go == 0)
	{
		if (is_dedicated_server)
			conOutput.Print(NULL, "teams balancing finished\n", MType::msg_info);
		else
			conOutput.Print(NULL, "Teams balancing finished\n", MType::msg_info);

		// set those on last run
		botmanager.ResetTeamsBalanceValue();
		state = 0;
		bots_to_go = -1;

		return 1;
	}

	if (state == 0)
		return 0;
	else
		return -1;
}
// SECTION BY kota@ END


/*
* changes bot skill level to all bots
* by_one == true means increase/decrease by one level
* skill_level == -1 means decreasing and == 1 is increasing
*/
int utils_t::ChangeBotSkillLevel(bool by_one, int skill_level)
{
	// increase/decrease by one level
	if (by_one)
	{
		bool no_bots = TRUE;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// skip unused slots
			if (bots[i].is_used == false)
				continue;

			// skip bots with highest skill while increasing
			if ((bots[i].GetBotSkill() == 0) && (skill_level == 1))
				continue;

			// skip bots with lowest skill while decreasing
			if ((bots[i].GetBotSkill() == BOT_SKILL_LEVELS - 1) && (skill_level == -1))
				continue;

			// found atleast one bot to process
			no_bots = FALSE;

			// store current skill level
			int prev_level = bots[i].GetBotSkill();
			
			// increase skill
			if (skill_level == 1)
				prev_level--;
			// decrease skill
			else if (skill_level == -1)
				prev_level++;

			// set new skill level
			bots[i].SetBotSkill(prev_level);
		}

		if (no_bots)
			return 0;
		else
			return 1;
	}
	// otherwise set to skill_level
	else if (by_one == false)
	{
		skill_level -= 1;		// due array based style

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// skip unused slots
			if (bots[i].is_used == false)
				continue;

			bots[i].SetBotSkill(skill_level);
		}

		return skill_level + 1;
	}

	// some error occured
	return -1;
}


/*
* changes aim skill level to all bots
*/
int utils_t::ChangeAimSkillLevel(int skill_level)
{
	skill_level -= 1;		// due array based style

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// skip unused slots
		if (bots[i].is_used == false)
			continue;

		bots[i].SetAimSkill(skill_level);
	}

	return skill_level + 1;
}


/*
* translates weapon name to weapon ID value and returns the ID value
*/
// This should handle all the if (g_mod_version ==) however this would also increase CPU load
// because it would have to go throuh the array with each if (pBot->current_weapon.iId == )
/*/
int UTIL_GetIDFromName(const char *weapon_name)
{
	extern bot_weapon_t weapon_defs[MAX_WEAPONS];

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if (strcmp(weapon_defs[i].szClassname, weapon_name) == 0)
		{
			return weapon_defs[i].iId;
		}
	}

#ifdef _DEBUG
	char msg[256];
	sprintf(msg, "\"%s\" isn't valid weapon name or this weapon doesn't exist in currect mod",
		weapon_name);
	util.DebugInFile(msg);
#endif

	// ensures false evaluation
	return -1;
}
/**/


// check if bot is in wpt_index wpt range		CURRENTLY NOT USED
// if bot is in range return wpt_index range otherwise return 9999.0
/*float UTIL_IsInRange(edict_t *pEdict, int wpt_index)
{
	// is it valid
	if (wpt_index != -1)
	{
		float dist = (pEdict->v.origin - waypoints[wpt_index].origin).Length();

		// is in range or really close to it
		if (dist < waypoints[wpt_index].range + 10.0)
			return waypoints[wpt_index].range;
	}

	return (float) 9999.0;
}*/


/*
* returns the right direction to climb this ladder based on ladder vs bot origin z-coord		CURRENTLY NOT USED
*/
/*/
int UTIL_GetLadderDir(bot_t *pBot)
{
	edict_t *pent = NULL;

	while ((pent = UTIL_FindEntityInSphere(pent, pBot->pEdict->v.origin, STANDARD_SEARCH_RADIUS)) != NULL)
	{
		if (strcmp("func_ladder", STRING(pent->v.classname)) == 0)
		{
			Vector ladder_origin = VecBModelOrigin(pent);

#ifdef _DEBUG
			if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
			{
				ALERT(at_console, "LADDER - bot z-coord=%.1f | ladder z-coord=%.1f)\n",
					(float) pBot->pEdict->v.origin.z, (float) ladder_origin.z);
			}
#endif

			// the bot has to climb down
			if (ladder_origin.z < pBot->pEdict->v.origin.z)
				return LADDER_DOWN;
			// the bot has to climb up
			else if (ladder_origin.z >= pBot->pEdict->v.origin.z)
				return LADDER_UP;
		}
	}

	return LADDER_UNKNOWN;
}
/**/


/*
* returns TRUE if a traceline sent from the start reached destination
* allows ignoring any player entity that it hits vie the switch
*/
bool utils_t::IsPointReachable(const Vector& vecStart, const Vector& vecDestination, bool ignore_players)
{
	TraceResult tr;

	if (ignore_players)
	{
		TraceLineIgnoringPlayers(vecStart, vecDestination, &tr);

		if (tr.flFraction == 1.0f)
			return true;
	}
	else
	{
		UTIL_TraceLine(vecStart, vecDestination, dont_ignore_monsters, dont_ignore_glass, NULL, &tr);

		if (tr.flFraction == 1.0f)
			return true;
	}

	return false;
}


/*
* returns TRUE if given entity is in given radius around bot and sets a pointer to it
*/
bool utils_t::IsEntityNearby(bot_t* pBot, const char* entity_classname, float radius)
{
	edict_t* pent = NULL;

	// search the surrounding for entities
	while ((pent = util.FindEntityInSphere(pent, pBot->pEdict->v.origin, radius)) != NULL)
	{
		// skip self
		if (pent == pBot->pEdict)
			continue;

		// skip everything that doesn't match given entity name
		if (util.IsEntityName(pent, entity_classname) == false)
			continue;

		TraceResult tr;
		Vector v_src = pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs;
		Vector v_end;

		// first handle the entities with zero origin
		if (pent->v.origin == g_vecZero)
			v_end = VecBModelOrigin(pent);
		else
			v_end = pent->v.origin;

		UTIL_TraceLine(v_src, v_end, dont_ignore_monsters, pBot->pEdict, &tr);

		// can bot see it? (the traceline either hit the entity we are looking for OR reached its centre in case of entities with zero origin)
		if ((tr.pHit == pent) || (tr.flFraction == 1.0f))
		{
			// then store the pointer to this entity
			pBot->SetPointerToGEnt(pent);

			return true;
		}
	}

	return false;
}


/*
* returns TRUE if there is a capture area in given radius around bot and sets a pointer to it
*/
bool utils_t::IsCaptureAreaNearby(bot_t* pBot, float radius)
{
	edict_t* pent = NULL;

	// search the surrounding for entities
	while ((pent = util.FindEntityInSphere(pent, pBot->pEdict->v.origin, radius)) != NULL)
	{
		// skip self
		if (pent == pBot->pEdict)
			continue;

		// skip everything that doesn't match given entity name
		if (util.IsEntityName(pent, "dod_capture_area") == false)
			continue;

		// were we searching close vicinity?
		if (radius == STANDARD_SEARCH_RADIUS)
		{
			pBot->SetPointerToGEnt(pent);

			// then it must be the entity we were looking for
			return true;
		}

		// otherwise make sure bot can really see it so...

		TraceResult tr;
		Vector v_src = pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs;

		// this entity has zero origin so we have to deal with it this way
		UTIL_TraceLine(v_src, VecBModelOrigin(pent), ignore_monsters, pBot->pEdict, &tr);

		// can bot see it? (we can't check for pointers here, because in this case it returns 'worldspawn', probably due to the fact it's a non solid zone/area, so no collisions)
		if (tr.flFraction == 1.0f)
		{
			// then store the pointer to this entity
			pBot->SetPointerToGEnt(pent);

			return true;
		}
	}

	return false;
}


/*
* returns TRUE if there is a teammate found in the vicinity, actually the exact number of teammates we were looking for
*/
bool utils_t::IsTeammateNearby(bot_t* pBot, float radius, int num_of_teamates, bool ignore_fireteam)
{
	edict_t* pent = NULL;
	int found_teammates = 0;

	// search the surrounding for entities
	while ((pent = util.FindEntityInSphere(pent, pBot->pEdict->v.origin, radius)) != NULL)
	{
		if (pent == pBot->pEdict)
			continue;

		// skip everything that's not a player entity
		if (util.IsEntityName(pent, "player") == false)
			continue;

		// skip all enemies
		if (util.GetTeam(pent) != pBot->GetBotTeam())
			continue;

		// do we have to ignore the teammates that are in the fireteam this bot formed?
		if (ignore_fireteam && (GetBotIndex(pent) != NO_VAL) && (bots[GetBotIndex(pent)].pTeamLeader == pBot->pEdict))
			continue;

		TraceResult tr;
		Vector v_src = pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs;

		UTIL_TraceLine(v_src, pent->v.origin, dont_ignore_monsters, pBot->pEdict, &tr);

		// can bot see this teammate (not hidden behind wall/sandbag/whatever)? ie. bot shouldn't cheat and detect teammates through solid objects
		if (tr.pHit == pent)
		{
			found_teammates++;

			// store the pointer to this teammate if there is no pointer to any other game entity
			if (pBot->HasNoGEnt())
				pBot->SetPointerToGEnt(pent);

			// did we find the number of teammates we were looking for?
			if (found_teammates == num_of_teamates)
				return true;
		}
	}

	return false;
}


/*
* returns TRUE if there is any explosives charge in given radius around bot or given start point and sets a pointer to it
*/
bool utils_t::IsExplosivesChargeNearby(bot_t* pBot, float radius, const Vector custom_start_point)
{
	edict_t* pent = NULL;
	Vector start_point;

	if (custom_start_point != g_vecZero)
		start_point = custom_start_point;
	else
		start_point = pBot->pEdict->v.origin;

	// search the surrounding for entities
	while ((pent = util.FindEntityInSphere(pent, start_point, radius)) != NULL)
	{
		// skip self
		if (pent == pBot->pEdict)
			continue;

		// skip everything that isn't explosives charge
		if (util.IsEntityName(pent, "dod_object") == false)
			continue;

		// skip those that aren't really there (the satchel/bomb isn't rendered when someone already took it)
		if (pent->v.effects & EF_NODRAW)
			continue;

		TraceResult tr;
		Vector v_src = pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs;

		UTIL_TraceLine(v_src, pent->v.origin, ignore_monsters, pBot->pEdict, &tr);

		// can bot see it?
		if ((tr.pHit == pent) || (tr.flFraction == 1.0f))
		{
			// then store the pointer to this entity
			pBot->SetPointerToGEnt(pent);

			return true;
		}
	}
	
	return false;
}


/*
* defines which DoD classes can call the "I need backup" voice command and so form the fireteam
*/
bool utils_t::CanBeFireTeamLeader(bot_t* pBot)
{
	return ((pBot->IsBotTeam(teamONE.GetTeamId()) && (internals.IsBritishTeam() ? (pBot->GetBotClass() == 2) : (pBot->GetBotClass() == 3))) ||
		(pBot->IsBotTeam(teamTWO.GetTeamId()) && (pBot->GetBotClass() == 3)));
}


/*
* returns current count of members in the fireteam, actually the number of bots that follow this player/bot because the team leader isn't counted
*/
int utils_t::CountFireTeamMembers(edict_t* pLeader)
{
	int team_member_count = 0;

	// count how many team members follow this team leader
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (bots[i].pTeamLeader == pLeader)
			team_member_count++;

		// no point to continue we got all we need, this team is full already
		if (team_member_count == FIRETEAM_SIZE - 1)
			break;
	}

	return team_member_count;
}


/*
* returns TRUE if this bot is able to capture the area he entered/sees
*/
bool utils_t::CanBotCaptureTheArea(bot_t* pBot, bool ignore_teammate_requirement)
{
	// no pointer to any capture area yet AND no capture area nearby? then break it right away
	if (pBot->HasNoGEnt() && (IsCaptureAreaNearby(pBot, STANDARD_SEARCH_RADIUS) == false))
	{

#ifdef _DEBUG
		conOutput.Notify("CanCaptureTheArea() -> HasNoGameEnt and found nothing nearby\n");
#endif


		return false;
	}

	// try to find this capture area in the array of all capture areas
	int caparea_index = dodCaptureArea->FindPointInArray(pBot->GetPointerToGEnt());

	int bot_team = pBot->GetBotTeam();

	// now see whether this area needs to be captured or not anymore (ie. is already held by bot's team)
	if ((caparea_index != CAPTUREPOINTS_ERROR_VAL) && (dodCaptureArea->GetOwnedByTeam(caparea_index) != bot_team))
	{
		// can this area be captured by the team this bot is in?
		if ((dodCaptureArea->GetTeamOneAllowedToCapture(caparea_index) && pBot->IsBotTeam(teamONE.GetTeamId())) ||
			(dodCaptureArea->GetTeamTwoAllowedToCapture(caparea_index) && pBot->IsBotTeam(teamTWO.GetTeamId())))
		{
			// does this area require any goal item (eg. explosives charge) to capture AND this bot does NOT have any?
			if (dodCaptureArea->GetDodObjectRequired(caparea_index) && (pBot->IsTask(TASK_GOALITEM) == false))
				return false;	// then he's unable to capture it at all

			// first init them so they don't match even if something went wrong
			int num_of_players_currently_present = 0;
			int num_of_players_needed_to_cap = -1;

			if (bot_team == teamONE.GetTeamId())
			{
				num_of_players_currently_present = dodCaptureArea->GetTeamOnePlayersCurrPresent(caparea_index);
				num_of_players_needed_to_cap = dodCaptureArea->GetTeamOnePlayersToCapture(caparea_index);
			}
			else if (bot_team == teamTWO.GetTeamId())
			{
				num_of_players_currently_present = dodCaptureArea->GetTeamTwoPlayersCurrPresent(caparea_index);
				num_of_players_needed_to_cap = dodCaptureArea->GetTeamTwoPlayersToCapture(caparea_index);
			}

			// is it a disabled capture area via the settings in external file?
			if (num_of_players_needed_to_cap == 0)
				return false;

			// is it a one man capture area OR is there enough players to capture it?
			if ((num_of_players_needed_to_cap == 1) || (num_of_players_currently_present == num_of_players_needed_to_cap))
			{
				return true;
			}

			// is it an area that requires more players to capture and there isn't enough of them at the moment?
			if (num_of_players_currently_present < num_of_players_needed_to_cap)
			{
				// are we ignoring this requirement OR is there a teammate (enough of them) nearby?
				if (ignore_teammate_requirement || IsTeammateNearby(pBot, TEAMMATE_SEARCH_RADIUS, num_of_players_needed_to_cap - 1))// this bot counts as one so look for the rest we need
				{
					return true;
				}
			}
		}
	}

	return false;
}


/*
* returns true if this entity cannot be smashed by gunfire or knife stab and such
* DoD specific spawnflags for breakable entity seem to be: 16 = only Allies can kill it, 32 = only Axis can kill it, 512 = rocket launcher can kill it
* because the tanks on map Jagd have spawnflags set to 544 which is 512 + 32 and only the Axis rocket launcher can destroy them
* and the radar panels and other objects on map Glider have spawnflags set to 528 which is 512 + 16 and only Allied bazooka can destroy them
* 
* and there is also an unknown flag
* 1024 on some bunkers on map forest - actually there is 1536 which means 512 as a rocket launcher + 1024 ie. 1<<10 as some unknown flag
*/
bool utils_t::NotBreakableByGunfire(edict_t* pEdict)
{
	return ((pEdict->v.spawnflags & (SF_BREAK_TRIGGER_ONLY | SF_BREAK_OBJECT_CAP_ONLY)) || (pEdict->v.takedamage == DAMAGE_NO) || (pEdict->v.health < 0.0f));
}


/*
* returns TRUE if the entity has 'immune water' flag, because
* DoD seems to flag breakable objects based on func breakable that can only be broken by explosives (satchel or tnt or rocket launcher) with that flag (ie. 1<<17)
*/
bool utils_t::IsEntityBreakableByExplosivesOnly(edict_t* pEntity)
{
	return (pEntity->v.flags & FL_IMMUNE_WATER);
}


/*
* returns TRUE if given team doesn't match DoD specific team spawnflag for given entity
*/
bool utils_t::NotBreakableByThisTeam(edict_t* pEntity, int team)
{
	return (((pEntity->v.spawnflags & SF_BREAK_ALLIES_ONLY) && (team == teamTWO.GetTeamId())) || ((pEntity->v.spawnflags & SF_BREAK_AXIS_ONLY) && (team == teamONE.GetTeamId())));
}


/*
* returns true if there's a breakable object in front of the bot
*/
bool utils_t::CheckForwardForBreakable(bot_t* pBot, bool check_vicinity)
{
	Vector v_src, v_dest;
	TraceResult tr;

	UTIL_MakeVectors(pBot->pEdict->v.v_angle);
	v_src = pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs;

	// did we find another breakable entity in previous check? then use it and again allow subsequent checking for possible other parts of this target object
	if (pBot->IsPointerToGEntThisEntity("func_breakable") && (NotBreakableByGunfire(pBot->GetPointerToGEnt()) == false))
	{
		//v_dest = VecBModelOrigin(pBot->GetPointerToGEnt());
		v_dest = VecAbsoluteOrigin(pBot->GetPointerToGEnt());// gives slightly different coords, but it doesn't seem to make the bot significantly more accurate
		check_vicinity = true;
		pBot->RemoveSubTask(ST_AIM_DONE);
		pBot->RemoveSubTask(ST_FACEPOINTIS_DONE);
	}
	// do we have the coordinates of the target from teammate? then make a vector towards these coords, but extend it a little so that the Traceline can actually reach and hit the target
	else if (pBot->GetPositionOfPointInSpace() != g_vecZero)
	{
		v_dest = v_src + (pBot->GetPositionOfPointInSpace() - v_src).Normalize() * ((pBot->GetPositionOfPointInSpace() - v_src).Length() * 1.01f);
		check_vicinity = true;		
		pBot->RemoveSubTask(ST_AIM_DONE);
		pBot->RemoveSubTask(ST_FACEGENT_DONE);
	}
	// is there a priority 1 setting used on the nearby aim waypoint? then double the distance to look for the breakable object and also look for possible other parts of this target
	else if (wptmanager.IsWaypointTypeTeamPriority(pBot->GetCurrentAimWaypoint(), WptT::aim, 1, pBot->GetBotTeam()))
	{
		v_dest = v_src + gpGlobals->v_forward * (EXTENDED_SEARCH_RADIUS * 2.0f);
		check_vicinity = true;
		pBot->RemoveSubTask(ST_FACEGENT_DONE);
		pBot->RemoveSubTask(ST_FACEPOINTIS_DONE);
	}
	// otherwise do common search in the direction of aim waypoint with default priority setting
	else
		v_dest = v_src + gpGlobals->v_forward * EXTENDED_SEARCH_RADIUS;
	
	UTIL_TraceLine(v_src, v_dest, dont_ignore_monsters, pBot->pEdict, &tr);

	if (IsEntityName(tr.pHit, "func_breakable"))
	{
		// cannot be destroyed by gunfire/knife or is already destroyed?
		if (NotBreakableByGunfire(tr.pHit))
			return false;

		// cannot be destroyed by the team this bot joined?
		if (NotBreakableByThisTeam(tr.pHit, pBot->GetBotTeam()))
			return false;

		// breakable by explosives only?
		if (IsEntityBreakableByExplosivesOnly(tr.pHit))
		{
			// bot doesn't carry a rocket/grenade launcher at all or has it but without ammo or is too close to this breakable?
			if ((pBot->IsBehaviour(AASPEC) == false) || (pBot->IsBehaviour(AASPEC) && (pBot->IsNoAmmoForMainWeapon() || (pBot->IsInSafeDistanceToShoot(tr.vecEndPos) == false))))
				return false;

			// let bots with rocket/grenade launcher know to switch to it in order to break this entity
			pBot->SetSubTask(ST_MEDEVAC_ST);
		}

		// use eyes origin to make correct aim vector in case this breakable object is low ... especially if bot is far away from it
		pBot->SetSubTask(ST_USEEYESORIGIN);

		// do we need to check for multiple parts of this breakable object? then see if there are other parts of the object around the hit point (detects objects like cracked door on dod_flash)
		if (check_vicinity)
			CheckForBreakableAround(pBot, STANDARD_SEARCH_RADIUS / 2.0f, tr.vecEndPos);

		return true;	// this is a suitable breakable object for this bot
	}
	// is there no func_breakable that can be destroyed? then try to discover possible other parts of the target object around the point to which the aiming vector points to
	else if (check_vicinity && (pBot->Aims.IsEmpty() == false) && (pBot->curr_wpt_index != NO_VAL))
	{
		// first we must prepare the aiming vector (ie. what the check shoot command shows)...
		v_src = waypoints[pBot->curr_wpt_index].origin + Vector(0, 0, 22);
		v_dest = v_src + (waypoints[pBot->GetCurrentAimWaypoint()].origin - v_src).Normalize() * (EXTENDED_SEARCH_RADIUS * 2.0f);

		// in order to do standard search around the end point
		if (CheckForBreakableAround(pBot, STANDARD_SEARCH_RADIUS, v_dest))
			return true;
	}
	// is there no func_breakable that can be destroyed? then try to discover possible other parts of the target object around the point to which teammate pointed to
	else if (check_vicinity && (pBot->GetPositionOfPointInSpace() != g_vecZero) && CheckForBreakableAround(pBot, STANDARD_SEARCH_RADIUS, pBot->GetPositionOfPointInSpace()))
		return true;

	return false;
}


/*
* returns true if there's a breakable object somewhere around the bot or given start point
*/
bool utils_t::CheckForBreakableAround(bot_t *pBot, float radius, const Vector custom_start_point)
{
	edict_t *pent = NULL;
	edict_t *pEdict = pBot->pEdict;
	Vector start_point;

	if (custom_start_point != g_vecZero)
		start_point = custom_start_point;	// do search around given position
	else
		start_point = pEdict->v.origin;		// by default do search around the bot

	// search the surroundings for entities
	while ((pent = util.FindEntityInSphere(pent, start_point, radius)) != NULL)
	{
		// handle only proper entities
		if (IsEntityName(pent, "func_breakable") == false)
			continue;

		// not breakable by gunfire/knife or anymore
		if (NotBreakableByGunfire(pent))
			continue;

		// cannot be destroyed by the team this bot joined?
		if (NotBreakableByThisTeam(pent, pBot->GetBotTeam()))
			continue;

		// will handle entities with origin 0,0,0
		Vector entity_origin = VecBModelOrigin(pent);
		
		// breakable by explosives only AND bot doesn't carry a rocket/grenade launcher at all or has it but without ammo or is too close to this breakable?
		if (IsEntityBreakableByExplosivesOnly(pent) && ((pBot->IsBehaviour(AASPEC) == false) ||
			(pBot->IsBehaviour(AASPEC) && (pBot->IsNoAmmoForMainWeapon() || (pBot->IsInSafeDistanceToShoot(entity_origin) == false)))))
			continue;

		TraceResult tr;
		Vector v_src = pEdict->v.origin + pEdict->v.view_ofs;

		// do we see the entity from here
		UTIL_TraceLine(v_src, entity_origin, dont_ignore_monsters, pEdict, &tr);

		if (IsEntityName(tr.pHit, "func_breakable") || ((tr.flFraction == 1.0f) && (pent->v.solid == SOLID_BSP)))// this should catch object with a hole in the middle
		{
			if (IsEntityName(tr.pHit, "worldspawn"))
				pBot->SetSubTask(ST_RANDOMCENTRE);

			// let bots with rocket/grenade launcher know to switch to it in order to break this entity
			if (IsEntityBreakableByExplosivesOnly(tr.pHit))
				pBot->SetSubTask(ST_MEDEVAC_ST);

			pBot->SetSubTask(ST_USEEYESORIGIN);
			
			// store the pointer to entity we found
			pBot->SetPointerToGEnt(pent);

			return true;
		}
	}

	return false;
}


/*
* returns true if there's destroyable object that can be destroyed only by explosives somewhere around the bot
*/
bool utils_t::CheckForClaymoreOnlySDObjectAround(bot_t* pBot)
{
	edict_t* pent = NULL;
	edict_t* pEdict = pBot->pEdict;

	// search close surrounding for entities, because the explosive charges cannot be thrown at the object or used remotely
	while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, STANDARD_SEARCH_RADIUS)) != NULL)
	{
		// ignore all entities except for these that are used to create the object for bombing
		if (IsEntityName(pent, "func_breakable") == false)
			continue;

		// ignore all func brekables that are either not for explosive charge or already broken or designed for the other team
		if (IsEntityName(pent, "func_breakable") && (IsEntityBreakableByExplosivesOnly(pent) == false) && NotBreakableByGunfire(pent) && NotBreakableByThisTeam(pent, pBot->GetBotTeam()))
			continue;

		Vector entity_origin = VecBModelOrigin(pent);

		TraceResult tr;
		Vector v_src = pEdict->v.origin + pEdict->v.view_ofs;

		// do we see the entity from here?
		UTIL_TraceLine(v_src, entity_origin, dont_ignore_monsters, pEdict, &tr);

		if (IsEntityName(tr.pHit, "func_breakable") || ((tr.flFraction == 1.0f) && (pent->v.solid == SOLID_BSP)))
		{
			// store the pointer to this entity
			pBot->SetPointerToGEnt(pent);

			return true;
		}
	}

	return false;
}


/*
* looks for any usable entity in the vicinity (e.g. button)
* returns true if bot should do any waypoint action that needs pressing the 'use' key
* returns fale if the bot didn't find anything, but we will hit the 'use' key anyway
* (in case there's special entity set up on some custom map that works like a button and requires pressing 'use' key)
*/
bool utils_t::CheckForUsablesAround(bot_t *pBot)
{
	edict_t *pent = NULL;
	Vector entity_origin;
	float search_radius;


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	if (botdebugger.IsDebugActions())
	{
		char dbgmsg[256]{};
		sprintf(dbgmsg, "%s - util.CheckForUsablesAround() called\n", pBot->name);
		conOutput.Notify(dbgmsg, pBot);
	}
#endif // DEBUG



	edict_t *pEdict = pBot->pEdict;

	// using local variable allows us to extend the search radius if we needed to use any entity from greater distance than standard search range
	search_radius = STANDARD_SEARCH_RADIUS;

	// search the surrounding for entities
	while ((pent = FindEntityInSphere(pent, pEdict->v.origin, search_radius)) != NULL)
	{
		if (IsEntityName(pent, "momentary_rot_button"))
		{
			// make vector to entity
			Vector entity = pent->v.origin - pEdict->v.origin;

			// make angles from vector to entity
			Vector bot_angles = UTIL_VecToAngles(entity);

			// look at the entity at correct angle
			pEdict->v.idealpitch = -bot_angles.x;
			BotFixIdealPitch(pEdict);

			// face the entity
			pEdict->v.ideal_yaw = bot_angles.y;
			BotFixIdealYaw(pEdict);

			// print what we just found if needed
			if (botdebugger.IsDebugActions())
			{
				conOutput.Notify("Found m_rot_button (wheel)\n", pBot);
			}

			// store pointer to the entity we found
			pBot->SetPointerToGEnt(pent);

			// NOTE: this should be changed to more complex time setting based on the difference of current view angle and angle to item

			// set some time to face the item
			pBot->SetTimeToFaceGEnt(0.5f);

			// no need to continue in searching
			return true;
		}

		// use BModel conversion for entities that are BModels (ie. those entities have 0,0,0 as an origin value)
		entity_origin = VecBModelOrigin(pent);

		if (IsEntityName(pent, "func_button") || IsEntityName(pent, "button_target"))
		{
			Vector entity = entity_origin - pEdict->v.origin;

			Vector bot_angles = UTIL_VecToAngles(entity);

			pEdict->v.idealpitch = -bot_angles.x;
			BotFixIdealPitch(pEdict);

			pEdict->v.ideal_yaw = bot_angles.y;
			BotFixIdealYaw(pEdict);

			if (botdebugger.IsDebugActions())
			{
				if (IsEntityName(pent, "func_button"))
					conOutput.Notify("Found button\n", pBot);
				else if (IsEntityName(pent, "button_target"))
					conOutput.Notify("Found button_target\n", pBot);
			}

			pBot->SetPointerToGEnt(pent);

			// NOTE: Same as above, it should be changed to more complex time setting based on the difference of current view angle and angle to item
			pBot->SetTimeToFaceGEnt(0.5f);

			return true;
		}
	}

	// didn't find anything but "press" the use key anyway
	return false;
}


/*
* we must add these methods when we are under linux
* you'll also have to add them when you use different compiler than one of MS VisualC compilers,
* because these methods are MSVC methods
*/
#ifdef __unix__
char *strrev(char *str);
char *strlwr(char *str);

/*
* this method will reverse the string
* ie. string "abc" will become "cba"
*/
char *strrev(char *str) 
{
	int i, j=strlen(str);
	char t;
	if (j==0) { return str; }
	for (i=0, --j; i<j; ++i,--j) 
	{
		t = str[i];
		str[i] = str[j];
		str[j] = t;
	}
	return str;
}


/*
* this method will change the string to all lower case
* ie. "HI There" will be "hi there"
*/
char *strlwr(char *str) 
{
	int len = strlen(str);
	for (int i=0; i<len; ++i)
	{
		str[i] = tolower(str[i]);
	}
	return str;
}
#endif


/*
* returns true if the string has at least 3 chars and doesn't include any of unwanted chars
*/
bool IsStringValid(char *str)
{
	if ((str != NULL) && (strlen(str) > 3) && (strstr(str, "<") == NULL) &&
		(strstr(str, ">") == NULL) && (strstr(str, "{") == NULL) && (strstr(str, "}") == NULL) &&
		(strstr(str, "[") == NULL) && (strstr(str, "]") == NULL) && (strstr(str, "(") == NULL) &&
		(strstr(str, ")") == NULL) && (strstr(str, "+") == NULL) && (strstr(str, "=") == NULL) &&
		(strstr(str, "_") == NULL) && (strstr(str, "|") == NULL) && (strstr(str, "~") == NULL) &&
		(strstr(str, "*") == NULL) && (strstr(str, "/") == NULL) && (strstr(str, "\\") == NULL))
		return true;

	return false;
}


/*
* returns a name without various clantags and similar stuff (it is usually a small part from the original nick)
*/
void ProcessTheName(char *name)
{
	// check name length and if it is too long try to cut it to something useful
	if ((name != NULL) && (strlen(name) > 6))
	{
		char temp_name[BOT_NAME_LEN + 1]{};
		char *useful_part;
		char *seperators = " <>)([]}{+=_~*|/\\";

		// reverse the name first so we can work with it from behind,
		// it will handle the problem with 3+ chars long clantags before names (doesn't work on 100%, however this way has much better results then if we read it from beginning)
		strrev(name);
		
		// cut the first part based on seperators
		useful_part = strtok(name, seperators);

		// check if this part is valid name
		if (IsStringValid(useful_part))
		{
			int len = strlen(useful_part);

			// check name ends and remove anything that isn't an alphabetical letter or digit
			if ((isalnum(useful_part[len-1])) == FALSE)
				useful_part[len-1] = '\0';

			strcpy(temp_name, useful_part);

			if (isalnum(temp_name[0]))
			{
				// finally reverse it back to original reading before returning it
				strcpy(name, strrev(temp_name));
				return;
			}
			else
			{
				strcpy(name, &temp_name[1]);
				strrev(name);
				return;
			}
		}
		// it wasn't valid so try to find something better in the rest of the name
		else
		{
			int safety_stop = 0;
			while ((useful_part = strtok(NULL, seperators)) != NULL)
			{
				if (IsStringValid(useful_part))
				{
					int len = strlen(useful_part);
					
					if ((isalnum(useful_part[len-1])) == FALSE)
						useful_part[len-1] = '\0';
					
					strcpy(temp_name, useful_part);
					
					if (isalnum(temp_name[0]))
					{
						strcpy(name, strrev(temp_name));
						return;
					}
					else
					{
						strcpy(name, &temp_name[1]);
						strrev(name);
						return;
					}
				}

				// if we can't find anything useful then use "you" instead of the name
				if (safety_stop > 10)
				{
					strcpy(name, "you");
					return;
				}

				safety_stop++;
			}

			// there wasn't anything else to cut after the first one so reverse the name back to original reading
			strrev(name);
		}
	}
}


/*
* returns only a few chars from the original name, handles nicks that passed previous processing and are still too long (prev. method coudln't handle them much)
*/
void ShortenIt(char *name)
{
	if (((name != NULL) && (strlen(name) <= 12)) || (name == NULL))
		return;

	// the name is still too long so cut it to only a few chars and make it low case
	char temp_name[BOT_NAME_LEN + 1];

	strncpy(temp_name, name, 4);
	temp_name[4] = '\0';
	strcpy(name, temp_name);
	name[4] = '\0';
	strcat(name, "...");
	strlwr(name);
}


/*
* returns a name that has been shortened and processed in a way so it looks more like if human wrote it (ie. without various clantags and similar stuff)
*/
void utils_t::HumanizeTheName(const char* original_name, char *name)
{
	if (original_name != NULL)
		strncpy(name, original_name, BOT_NAME_LEN);
	
	// just in case something went wrong
	if (name[0] == '\0')
		strcpy(name, "man");

	// remove clantags and pick only a part of the original nick
	ProcessTheName(name);
	// additional check to catch nicks that weren't shortened enough
	ShortenIt(name);
}


/*
* removes any unpritable character from given string
*/
void utils_t::RemoveIllegalCharsFromBuffer(char* buffer)
{
	int length = strlen(buffer);

	for (int str_pos = 0; str_pos < length; str_pos++)
	{
		if ((buffer[str_pos] < ' ') || (buffer[str_pos] > '~') || (buffer[str_pos] == '"'))
		{
			for (int i = str_pos; i < length; i++)
				buffer[i] = buffer[i + 1];
		}

		length--;
	}
}


/*
* removes any (Windows as well as Linux) newline character from the end of the buffer
*/
void utils_t::RemoveNewlineCharsFromBufferEnd(char* buffer)
{
	int length = strlen(buffer);

	if (buffer[length - 1] == '\n')
		buffer[length - 1] = 0;  // remove '\n'

	// now if we are on Linux we must check for the \r that may still be present at the end of the string if the source of it is Windows originally,
	// because the newline in a text file on Windows is marked by sequence of \r\n, while on Linux it is just \n
#ifdef __linux__
	length = strlen(buffer);

	if (buffer[length - 1] == '\r')
		buffer[length - 1] = 0;  // remove '\r'
#endif // __linux__
}


/*
* removes Marine Bot tag from the bot name
*/
void utils_t::RemoveTagsFromBotname(const char* bot_name, char* stripped_name)
{
	// if we are using the rich names or the name includes MB tag then we will have to copy the the original name that is behind MB tag
	if (externals.GetRichNames() || strstr(bot_name, "[MB]"))
		UTIL_StringFromBuffer(stripped_name, bot_name, ']', '\0');
	// otherwise just copy the name
	else
	{
		strcpy(stripped_name, bot_name);

		// just in case
		stripped_name[BOT_NAME_LEN] = 0;
	}
}


void utils_t::ShowMenu(edict_t* pEdict, int slots, int displaytime, bool needmore, char* pText)
{
	if (gmsgShowMenu == 0)
		gmsgShowMenu = REG_USER_MSG("ShowMenu", -1);

	pfnMessageBegin(MSG_ONE, gmsgShowMenu, NULL, pEdict);

	pfnWriteShort(slots);
	pfnWriteChar(displaytime);
	pfnWriteByte(needmore);
	pfnWriteString(pText);

	pfnMessageEnd();
}


/*
* do print weapon name based on its ID at console
*/
void utils_t::PrintAvailableWeapons(bot_t* pBot)
{
	extern edict_t* pRecipient;
	extern bot_weapon_t weapon_defs[MAX_WEAPONS];
	char main[64]{}, backup[64]{}, grenade[64]{}, claymore[64]{}, msg[128]{};
	int use_as_value = 0;
	static int anything_changed = -1;

	strcpy(main, "n/a");
	strcpy(backup, "n/a");
	strcpy(grenade, "n/a");
	strcpy(claymore, "n/a");

	if (pBot->main_weapon != NO_VAL)
	{
		// copy just the important part of the name of this weapon (in other words we'll ignore first 7 characters from the classname ... ie. weapon_)
		strcpy(main, StripWeaponName(weapon_name[pBot->main_weapon]));
	}

	if (pBot->backup_weapon != NO_VAL)
	{
		strcpy(backup, StripWeaponName(weapon_name[pBot->backup_weapon]));
	}

	if (pBot->grenade_slot != NO_VAL)
	{
		strcpy(grenade, StripWeaponName(weapon_name[pBot->grenade_slot]));
	}

	if (pBot->claymore_slot != NO_VAL)
	{
		strcpy(claymore, StripWeaponName(weapon_name[pBot->claymore_slot]));
	}


	if (pBot->IsUsedWeaponMain()) { use_as_value = 1; }
	else if (pBot->IsUsedWeaponBackup()) { use_as_value = 2; }
	else if (pBot->IsUsedWeaponKnife()) { use_as_value = 3; }
	else if (pBot->IsUsedWeaponGrenade()) { use_as_value = 4; }
	else if (pBot->IsUsedWeaponClaymoreMine()) { use_as_value = 5; }
	else { use_as_value = 0; }

	// print the message only if there was any change in weapon usage
	if (anything_changed != use_as_value)
	{
		anything_changed = use_as_value;

		sprintf(msg, "GUNS:main=<%s> backup=<%s> nade=<%s> mine=<%s> (using: %s)\n", main, backup, grenade, claymore, ConvertUsedWeaponToString(pBot));
		ClientPrint(pRecipient, HUD_PRINTNOTIFY, msg);
	}
}


char* utils_t::StripWeaponName(char* name_buffer)
{
	if (name_buffer == NULL)
		return "unknown";

	return &name_buffer[7];
}


char* utils_t::PrintWeaponName(int weaponID)
{
	if (weaponID == NO_VAL)
		return "unknown";

	return StripWeaponName(weapon_name[weaponID]);
}


char* utils_t::ConvertUsedWeaponToString(bot_t* pBot)
{
	if (pBot->IsUsedWeaponMain())
		return "main";
	if (pBot->IsUsedWeaponBackup())
		return "backup";
	if (pBot->IsUsedWeaponKnife())
		return "melee";
	if (pBot->IsUsedWeaponGrenade())
		return "grenade";
	if (pBot->IsUsedWeaponClaymoreMine())
		return "mine";

	return "unknown";
}


/*
* makes tracelines visible
*/
void utils_t::HighlightTrace(Vector v_source, Vector v_dest, edict_t *pEdict)
{
#ifdef _DEBUG
	extern edict_t *listenserver_edict;

	if (listenserver_edict && devTool.IsDisplayTracelines() && devTool.IsNotOverrideDisplayTL())
	{
		bool use_team_color = false;

		// edict isn't null AND has a team value specified then use his team color (well slightly modified ie. lighter shade)
		if (pEdict && (pEdict->v.team != 0))
		{
			devTool.ResetTLBeamColor();
			use_team_color = true;
		}

		Vector color = devTool.GetTLBeamColor();
		
		// if there's default color returned
		if (color == Vector(255, 255, 255))
		{
			if (use_team_color)
			{
				if (GetTeam(pEdict) == teamONE.GetTeamId())
					color = teamONE.GetTeamPathColor();
				else if (GetTeam(pEdict) == teamTWO.GetTeamId())
					color = teamTWO.GetTeamPathColor();

				// gives the beams slightly different shade to differ them from the team only paths
				if (color.x == 0)
					color.x = 50;
				if (color.y == 0)
					color.y = 50;
				if (color.z == 0)
					color.z = 50;
			}
			else
				color = devTool.GetTLBeamColor(true);// we don't want white Trace Lines
		}

		DrawBeam(listenserver_edict, v_source, v_dest, devTool.GetTLBeamDuration(), (int)color.x, (int)color.y, (int)color.z, 15);
	}
#endif
}


/*
* logging in public debug file
*/
void utils_t::DebugInFile(const char *msg)
{
	char filename[256]{};
	FILE *f;

	MarineBotFileName(filename, PUBLIC_DEBUG_FILE, NULL);

	f = fopen(filename, "a");
	
	fprintf(f, "***new record(time:%f)(map:%s)***\n", gpGlobals->time, STRING(gpGlobals->mapname));
	fprintf(f, msg);
	fprintf(f, "\n");
#ifndef DEBUG
	fprintf(f, "***record end***\n\n");
#else
	fprintf(f, "\n");
#endif // !DEBUG

	fclose(f);
}


/*
* logging in development debug file
*/
void utils_t::DebugDev(const char *msg, int wpt, int path)
{
#ifdef DEBUG

	FILE *f;

	// doesn't the debug file exist yet?
	if (debug_fname[0] == '\0')
	{
		// so build it
		MarineBotFileName(debug_fname, "!mb_devdebug.txt", NULL);

		ALERT(at_console, "DevelopmentDebug file was created !!!\n");
	}

	f = fopen(debug_fname, "a");
	
	fprintf(f, "***new record(time:%f)(map:%s)***\n", gpGlobals->time, STRING(gpGlobals->mapname));

	// print only valid messages
	if (msg != NULL)
	{
		fprintf(f, msg);
		fprintf(f, "\n");
	}
	
	if (wpt == -100)
		;
	else if (wpt < -1)
		fprintf(f, " wpt no. (some error, i.e. value < -1)\n");
	else if (wpt == -1)
		fprintf(f, " wpt no. %d (nothing/false)\n", wpt);
	else
		fprintf(f, " wpt no. %d\n", wpt + 1);
	
	if (path == -100)
		;
	else if (path < -1)
		fprintf(f, " path no. (some error, i.e. value < -1)\n");
	else if (path == -1)
		fprintf(f, " path no. %d (nothing/false)\n", path);
	else
		fprintf(f, " path no. %d\n", path + 1);
	
	fprintf(f, "***record end***\n\n");

	fclose(f);

	ALERT(at_console, "\n***EVENT SEND TO DEBUG FILE***\n");

#endif // DEBUG
}


/*
* logging in development debug file
*/
void utils_t::DebugDev(const char* msg, const char* string, int wpt, int path)
{
#ifdef DEBUG
	char joint_msg[512]{};

	sprintf(joint_msg, "%s (%s)\n", msg, string);
	DebugDev(joint_msg, wpt, path);
#endif // DEBUG
}


void DumpVector(FILE *f, Vector vec)
{
	fprintf(f, "Vector - x %.3f | y %.3f | z %.3f || Length %.3f\n", vec.x, vec.y, vec.z, vec.Length());
}


/*
* dumps all pEdict variables into the development debug file
* variables are in exact order as in progdefs.h
*/
void utils_t::DumpEdictToFile(edict_t *pEdict)
{
#ifdef _DEBUG

	FILE *f;

	DebugDev("***New dump call***", -100, -100);

	f = fopen(debug_fname, "a");
	
	fprintf(f, "***dump of pEdict(time:%f)***\n", gpGlobals->time);
	fprintf(f, "\n");
	
	if (pEdict->v.classname)
		fprintf(f, "classname %s\n", STRING(pEdict->v.classname));
	if (pEdict->v.globalname)
		fprintf(f, "globalname %s\n", STRING(pEdict->v.globalname));
	fprintf(f, "\n");

	fprintf(f, "origin ");
	DumpVector(f, pEdict->v.origin);
	fprintf(f, "oldorigin ");
	DumpVector(f, pEdict->v.oldorigin);
	fprintf(f, "velocity ");
	DumpVector(f, pEdict->v.velocity);
	fprintf(f, "basevelocity ");
	DumpVector(f, pEdict->v.basevelocity);
	fprintf(f, "clbasevelocity ");
	DumpVector(f, pEdict->v.clbasevelocity);
	fprintf(f, "\n");

	fprintf(f, "movedir ");
	DumpVector(f, pEdict->v.movedir);
	fprintf(f, "\n");

	fprintf(f, "angles ");
	DumpVector(f, pEdict->v.angles);
	fprintf(f, "avelocity ");
	DumpVector(f, pEdict->v.avelocity);
	fprintf(f, "punchangle ");
	DumpVector(f, pEdict->v.punchangle);
	fprintf(f, "v_angle ");
	DumpVector(f, pEdict->v.v_angle);
	fprintf(f, "\n");

	fprintf(f, "endpos ");
	DumpVector(f, pEdict->v.endpos);
	fprintf(f, "startpos ");
	DumpVector(f, pEdict->v.startpos);
	fprintf(f, "impacttime %.3f\n", pEdict->v.impacttime);
	fprintf(f, "starttime %.3f\n", pEdict->v.starttime);
	fprintf(f, "\n");

	fprintf(f, "fixangle %d\n", pEdict->v.fixangle);
	fprintf(f, "idealpitch %.3f\n", pEdict->v.idealpitch);
	fprintf(f, "pitch_speed %.3f\n", pEdict->v.pitch_speed);
	fprintf(f, "ideal_yaw %.3f\n", pEdict->v.ideal_yaw);
	fprintf(f, "yaw_speed %.3f\n", pEdict->v.yaw_speed);
	fprintf(f, "\n");

	fprintf(f, "modelindex %d\n", pEdict->v.modelindex);
	fprintf(f, "model %s\n", STRING(pEdict->v.model));
	fprintf(f, "\n");

	fprintf(f, "viewmodel %d\n", pEdict->v.viewmodel);
	fprintf(f, "weaponmodel %d\n", pEdict->v.weaponmodel);
	fprintf(f, "\n");

	fprintf(f, "absmin ");
	DumpVector(f, pEdict->v.absmin);
	fprintf(f, "absmax ");
	DumpVector(f, pEdict->v.absmax);
	fprintf(f, "mins ");
	DumpVector(f, pEdict->v.mins);
	fprintf(f, "maxs ");
	DumpVector(f, pEdict->v.maxs);
	fprintf(f, "size ");
	DumpVector(f, pEdict->v.size);
	fprintf(f, "\n");

	fprintf(f, "ltime %.3f\n", pEdict->v.ltime);
	fprintf(f, "nextthink %.3f\n", pEdict->v.nextthink);
	fprintf(f, "\n");

	fprintf(f, "movetype %d\n", pEdict->v.movetype);
	fprintf(f, "solid %d\n", pEdict->v.solid);
	fprintf(f, "\n");

	fprintf(f, "skin %d\n", pEdict->v.skin);
	fprintf(f, "body %d\n", pEdict->v.body);
	fprintf(f, "effects %d\n", pEdict->v.effects);
	fprintf(f, "\n");

	fprintf(f, "gravity %.3f\n", pEdict->v.gravity);
	fprintf(f, "friction %.3f\n", pEdict->v.friction);
	fprintf(f, "\n");

	fprintf(f, "light_level %d\n", pEdict->v.light_level);
	fprintf(f, "\n");

	fprintf(f, "sequence %d\n", pEdict->v.sequence);
	fprintf(f, "gaitsequence %d\n", pEdict->v.gaitsequence);
	fprintf(f, "frame %.3f\n", pEdict->v.frame);
	fprintf(f, "animtime %.3f\n", pEdict->v.animtime);
	fprintf(f, "framerate %.3f\n", pEdict->v.framerate);
	fprintf(f, "controller[0] %d\n", pEdict->v.controller[0]);
	fprintf(f, "controller[1] %d\n", pEdict->v.controller[1]);
	fprintf(f, "controller[2] %d\n", pEdict->v.controller[2]);
	fprintf(f, "controller[3] %d\n", pEdict->v.controller[3]);
	fprintf(f, "blending[0] %d\n", pEdict->v.blending[0]);
	fprintf(f, "blending[1] %d\n", pEdict->v.blending[1]);
	fprintf(f, "\n");
	
	fprintf(f, "scale %.3f\n", pEdict->v.scale);
	fprintf(f, "\n");

	fprintf(f, "rendermode %d\n", pEdict->v.rendermode);
	fprintf(f, "renderamt %.3f\n", pEdict->v.renderamt);
	fprintf(f, "rendercolor ");
	DumpVector(f, pEdict->v.rendercolor);
	fprintf(f, "renderfx %d\n", pEdict->v.renderfx);
	fprintf(f, "\n");


	fprintf(f, "health %.3f\n", pEdict->v.health);
	fprintf(f, "frags %.3f\n", pEdict->v.frags);
	fprintf(f, "weapons %d\n", pEdict->v.weapons);
	fprintf(f, "takedamage %.3f\n", pEdict->v.takedamage);
	fprintf(f, "\n");


	fprintf(f, "deadflag %d\n", pEdict->v.deadflag);
	fprintf(f, "view_ofs ");
	DumpVector(f, pEdict->v.view_ofs);
	fprintf(f, "\n");

	fprintf(f, "button %d\n", pEdict->v.button);
	fprintf(f, "impulse %d\n", pEdict->v.impulse);
	fprintf(f, "\n");

	// is exists
	if (pEdict->v.chain)
		fprintf(f, "chain (exists this is its name) %s\n", STRING(pEdict->v.chain->v.classname));
	else
		fprintf(f, "chain does not exist - no data\n");
	if (pEdict->v.dmg_inflictor)
		fprintf(f, "dmg_inflictor (exists this is its name) %s\n", STRING(pEdict->v.dmg_inflictor->v.classname));
	else
		fprintf(f, "dmg_inflictor does not exist - no data\n");
	if (pEdict->v.enemy)
		fprintf(f, "enemy (exists this is its name) %s\n", STRING(pEdict->v.enemy->v.classname));
	else
		fprintf(f, "enemy does not exist - no data\n");
	if (pEdict->v.aiment)
		fprintf(f, "aiment (exists this is its name) %s\n", STRING(pEdict->v.aiment->v.classname));
	else
		fprintf(f, "aiment does not exist - no data\n");
	if (pEdict->v.owner)
		fprintf(f, "owner (exists this is its name) %s\n", STRING(pEdict->v.owner->v.classname));
	else
		fprintf(f, "owner does not exist - no data\n");
	if (pEdict->v.groundentity)
		fprintf(f, "groundentity (exists this is its name) %s\n", STRING(pEdict->v.groundentity->v.classname));
	else
		fprintf(f, "groundentity does not exist - no data\n");
	fprintf(f, "\n");

	fprintf(f, "spawnflags %d\n", pEdict->v.spawnflags);
	fprintf(f, "flags %d\n", pEdict->v.flags);
	fprintf(f, "\n");

	fprintf(f, "colormap %d\n", pEdict->v.colormap);
	fprintf(f, "team %d\n", pEdict->v.team);
	fprintf(f, "\n");

	fprintf(f, "max_health %.3f\n", pEdict->v.max_health);
	fprintf(f, "teleport_time %.3f\n", pEdict->v.teleport_time);
	fprintf(f, "armortype %.3f\n", pEdict->v.armortype);
	fprintf(f, "armorvalue %.3f\n", pEdict->v.armorvalue);
	fprintf(f, "waterlevel %d\n", pEdict->v.waterlevel);
	fprintf(f, "watertype %d\n", pEdict->v.watertype);
	fprintf(f, "\n");

	if (pEdict->v.target)
		fprintf(f, "target %s\n", STRING(pEdict->v.target));
	else
		fprintf(f, "target does not exist - no data\n");
	if (pEdict->v.targetname)
		fprintf(f, "targetname %s\n", STRING(pEdict->v.targetname));
	else
		fprintf(f, "targetname does not exist - no data\n");
	if (pEdict->v.netname)
		fprintf(f, "netname %s\n", STRING(pEdict->v.netname));
	else
		fprintf(f, "netname does not exist - no data\n");
	if (pEdict->v.message)
		fprintf(f, "message %s\n", STRING(pEdict->v.message));
	else
		fprintf(f, "message does not exist - no data\n");
	fprintf(f, "\n");

	fprintf(f, "dmg_take %.3f\n", pEdict->v.dmg_take);
	fprintf(f, "dmg_save %.3f\n", pEdict->v.dmg_save);
	fprintf(f, "dmg %.3f\n", pEdict->v.dmg);
	fprintf(f, "dmgtime %.3f\n", pEdict->v.dmgtime);
	fprintf(f, "\n");

	if (pEdict->v.noise)
		fprintf(f, "noise %s\n", STRING(pEdict->v.noise));
	else
		fprintf(f, "noise does not exist - no data\n");
	if (pEdict->v.noise1)
		fprintf(f, "noise1 %s\n", STRING(pEdict->v.noise1));
	else
		fprintf(f, "noise1 does not exist - no data\n");
	if (pEdict->v.noise2)
		fprintf(f, "noise2 %s\n", STRING(pEdict->v.noise2));
	else
		fprintf(f, "noise2 does not exist - no data\n");
	if (pEdict->v.noise3)
		fprintf(f, "noise3 %s\n", STRING(pEdict->v.noise3));
	else
		fprintf(f, "noise3 does not exist - no data\n");
	fprintf(f, "\n");

	fprintf(f, "speed %.3f\n", pEdict->v.speed);
	fprintf(f, "air_finished %.3f\n", pEdict->v.air_finished);
	fprintf(f, "pain_finished %.3f\n", pEdict->v.pain_finished);
	fprintf(f, "radsuit_finished %.3f\n", pEdict->v.radsuit_finished);
	fprintf(f, "\n");

	if (pEdict->v.pContainingEntity)
		fprintf(f, "pContainingEntity (exists this is its name) %s\n", STRING(pEdict->v.pContainingEntity->v.classname));
	else
		fprintf(f, "pContainingEntity does not exist - no data\n");
	fprintf(f, "\n");

	fprintf(f, "playerclass %d\n", pEdict->v.playerclass);
	fprintf(f, "maxspeed %.3f\n", pEdict->v.maxspeed);
	fprintf(f, "\n");

	fprintf(f, "fov %.3f\n", pEdict->v.fov);
	fprintf(f, "weaponanim %d\n", pEdict->v.weaponanim);
	fprintf(f, "\n");

	fprintf(f, "pushmsec %d\n", pEdict->v.pushmsec);
	fprintf(f, "\n");

	fprintf(f, "bInDuck %d\n", pEdict->v.bInDuck);
	fprintf(f, "flTimeStepSound %d\n", pEdict->v.flTimeStepSound);
	fprintf(f, "flSwimTime %d\n", pEdict->v.flSwimTime);
	fprintf(f, "flDuckTime %d\n", pEdict->v.flDuckTime);
	fprintf(f, "iStepLeft %d\n", pEdict->v.iStepLeft);
	fprintf(f, "flFallVelocity %.3f\n", pEdict->v.flFallVelocity);
	fprintf(f, "\n");

	fprintf(f, "gamestate %d\n", pEdict->v.gamestate);
	fprintf(f, "\n");

	fprintf(f, "oldbuttons %d\n", pEdict->v.oldbuttons);
	fprintf(f, "\n");

	fprintf(f, "groupinfo %d\n", pEdict->v.groupinfo);
	fprintf(f, "\n");

	fprintf(f, "iuser1 %d\n", pEdict->v.iuser1);
	fprintf(f, "iuser2 %d\n", pEdict->v.iuser2);
	fprintf(f, "iuser3 %d\n", pEdict->v.iuser3);
	fprintf(f, "iuser4 %d\n", pEdict->v.iuser4);
	fprintf(f, "fuser1 %.3f\n", pEdict->v.fuser1);
	fprintf(f, "fuser2 %.3f\n", pEdict->v.fuser2);
	fprintf(f, "fuser3 %.3f\n", pEdict->v.fuser3);
	fprintf(f, "fuser4 %.3f\n", pEdict->v.fuser4);
	fprintf(f, "vuser1 ");
	DumpVector(f, pEdict->v.vuser1);
	fprintf(f, "vuser2 ");
	DumpVector(f, pEdict->v.vuser2);
	fprintf(f, "vuser3 ");
	DumpVector(f, pEdict->v.vuser3);
	fprintf(f, "vuser4 ");
	DumpVector(f, pEdict->v.vuser4);
	if (pEdict->v.euser1)
		fprintf(f, "euser1 (exists this is its name) %s\n", STRING(pEdict->v.euser1->v.classname));
	else
		fprintf(f, "euser1 does not exist - no data\n");
	if (pEdict->v.euser2)
		fprintf(f, "euser2 (exists this is its name) %s\n", STRING(pEdict->v.euser2->v.classname));
	else
		fprintf(f, "euser2 does not exist - no data\n");
	if (pEdict->v.euser3)
		fprintf(f, "euser3 (exists this is its name) %s\n", STRING(pEdict->v.euser3->v.classname));
	else
		fprintf(f, "euser3 does not exist - no data\n");
	if (pEdict->v.euser4)
		fprintf(f, "euser4 (exists this is its name) %s\n", STRING(pEdict->v.euser4->v.classname));
	else
		fprintf(f, "euser4 does not exist - no data\n");

	fprintf(f, "***record end***\n\n");
	fclose(f);

	ALERT(at_console, "\n***DONE***\n");

#endif	// _DEBUG

}


/*
* updates the count of players (both real clients as well as bots) for both teams
*/
void utils_t::CountPlayersInBothTeams(void)
{
	teamOne_player_count = 0;
	teamTwo_player_count = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		if (clients[i].pEntity)
		{
			if (GetTeam(clients[i].pEntity) == teamONE.GetTeamId())
				teamOne_player_count++;
			else if (GetTeam(clients[i].pEntity) == teamTWO.GetTeamId())
				teamTwo_player_count++;
		}
	}
}
