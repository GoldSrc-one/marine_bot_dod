///////////////////////////////////////////////////////////////////////////////////////////////
//
//	-- GNU -- open source 
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
// engine.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "bot_client.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "engine.h"
#ifdef DEBUG
#include "console_output.h"
#endif // DEBUG


extern enginefuncs_t g_engfuncs;

int debug_engine = 0;
char debug_fname[256];		// allow debugging into any location within HL folder

static bool dont_dump_this_message = false;		// prevents printing such message to the debugging file (useful to ignore messages that gets sent in every frame)

void (*botMsgFunction)(void *, int) = NULL;
void (*botMsgEndFunction)(void *, int) = NULL;
int botMsgIndex;

// messages created in RegUserMsg which will be "caught"
int message_VGUI = 0;
int message_ShowMenu = 0;
int message_WeaponList = 0;
int message_CurWeapon = 0;
int message_AmmoX = 0;
int message_AmmoShort = 0;	// looks like this is a unique message only for weapon mg34
int message_WeapPickup = 0;
int message_AmmoPickup = 0;
int message_ItemPickup = 0;
int message_Health = 0;
int message_Damage = 0;
int message_DeathMsg = 0;
int message_TextMsg = 0;
int message_FOV = 0;
int message_ScreenFade = 0;
int message_MOTD = 0;
int message_Object = 0;
int message_ClientAreas = 0;
int message_ReloadDone = 0;
int message_HandSignal = 0;

// following messages are messages to all (ie. dest=2 in debuging file)
int message_InitObj = 0;
int message_SetObj = 0;
int message_PlayersIn = 0;
int message_RoundState = 0;
int message_CapMsg = 0;


static FILE *fp;


#ifndef NEWSDKAM
// if you're getting errors here then go to defines.h and change the NEWSDKAM setting
int pfnPrecacheModel(char* s)
#else
int pfnPrecacheModel(const char* s)
#endif
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPrecacheModel: %s\n",s); fclose(fp); }
	return (*g_engfuncs.pfnPrecacheModel)(s);
}
#ifndef NEWSDKAM
int pfnPrecacheSound(char* s)
#else
int pfnPrecacheSound(const char* s)
#endif
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPrecacheSound: %s\n",s); fclose(fp); }
	return (*g_engfuncs.pfnPrecacheSound)(s);
}
void pfnSetModel(edict_t *e, const char *m)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetModel: edict=%p %s\n",e,m); fclose(fp); }
	(*g_engfuncs.pfnSetModel)(e, m);
}
int pfnModelIndex(const char* m)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnModelIndex: %s\n",m); fclose(fp); }
	return (*g_engfuncs.pfnModelIndex)(m);
}
int pfnModelFrames(int modelIndex)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnModelFrames: %d\n",modelIndex); fclose(fp); }
	return (*g_engfuncs.pfnModelFrames)(modelIndex);
}
void pfnSetSize(edict_t *e, const float *rgflMin, const float *rgflMax)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetSize: %x rgflMin: %f rgflMax: %f\n",e,*rgflMin,*rgflMax); fclose(fp); }
	(*g_engfuncs.pfnSetSize)(e, rgflMin, rgflMax);
}
#ifndef NEWSDKAM
void pfnChangeLevel(char* s1, char* s2)
#else
void pfnChangeLevel(const char* s1, const char* s2)
#endif
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnChangeLevel: s1: %s s2: %s\n",s1,s2); fclose(fp); }
	
	// kick any bot off of the server after time/frag limit...
	for (int index = 0; index < MAX_CLIENTS; index++)
	{
		if (bots[index].is_used)  // is this slot used?
		{
			char cmd[40];
			
			sprintf(cmd, "kick \"%s\"\n", bots[index].name);
			bots[index].respawn_state = RESPAWN_NEED_TO_RESPAWN;
			
			SERVER_COMMAND(cmd);  // kick the bot using (kick "name")
		}
	}
	
	(*g_engfuncs.pfnChangeLevel)(s1, s2);
}
void pfnGetSpawnParms(edict_t *ent)
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetSpawnParms: edict=%p\n",ent); fclose(fp); }
#endif
	(*g_engfuncs.pfnGetSpawnParms)(ent);
}
void pfnSaveSpawnParms(edict_t *ent)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSaveSpawnParms: edict=%p\n",ent); fclose(fp); }
#endif
   (*g_engfuncs.pfnSaveSpawnParms)(ent);
}
float pfnVecToYaw(const float *rgflVector)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnVecToYaw: rgflVector: %f\n",*rgflVector); fclose(fp); }
   return (*g_engfuncs.pfnVecToYaw)(rgflVector);
}
void pfnVecToAngles(const float *rgflVectorIn, float *rgflVectorOut)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnVecToAngles: rgflVectorIn: %f rgflVectorOut: %f\n",*rgflVectorIn,*rgflVectorOut); fclose(fp); }
   (*g_engfuncs.pfnVecToAngles)(rgflVectorIn, rgflVectorOut);
}
void pfnMoveToOrigin(edict_t *ent, const float *pflGoal, float dist, int iMoveType)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnMoveToOrigin: edict=%p pflGoal: %f dist: %f iMoveType: %d\n",ent,*pflGoal,dist,iMoveType); fclose(fp); }
#endif
   (*g_engfuncs.pfnMoveToOrigin)(ent, pflGoal, dist, iMoveType);
}
void pfnChangeYaw(edict_t* ent)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnChangeYaw: edict=%p\n",ent); fclose(fp); }
#endif
   (*g_engfuncs.pfnChangeYaw)(ent);
}
void pfnChangePitch(edict_t* ent)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnChangePitch: edict=%p\n",ent); fclose(fp); }
#endif
   (*g_engfuncs.pfnChangePitch)(ent);
}
edict_t* pfnFindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue)
{
	if (debug_engine)
	{
		fp=fopen(debug_fname,"a");
		if (pEdictStartSearchAfter && pEdictStartSearchAfter->v.classname)
			fprintf(fp,"pfnFindEntityByString: (ent classname: %s) field=%s value=%s\n", STRING(pEdictStartSearchAfter->v.classname), pszField, pszValue);
		else if (strcmp(pszValue, "info_firearms_detect") == 0)
			;	// do nothing ie. don't print it into the debugging file
		else
			fprintf(fp,"pfnFindEntityByString: field=%s value=%s\n", pszField, pszValue);
		fclose(fp);
	}

	return (*g_engfuncs.pfnFindEntityByString)(pEdictStartSearchAfter, pszField, pszValue);
}
int pfnGetEntityIllum(edict_t* pEnt)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetEntityIllum:\n"); fclose(fp); }
	return (*g_engfuncs.pfnGetEntityIllum)(pEnt);
}
edict_t* pfnFindEntityInSphere(edict_t *pEdictStartSearchAfter, const float *org, float rad)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnFindEntityInSphere:\n"); fclose(fp); }
	return (*g_engfuncs.pfnFindEntityInSphere)(pEdictStartSearchAfter, org, rad);
}
edict_t* pfnFindClientInPVS(edict_t *pEdict)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnFindClientInPVS:\n"); fclose(fp); }
	return (*g_engfuncs.pfnFindClientInPVS)(pEdict);
}
edict_t* pfnEntitiesInPVS(edict_t *pplayer)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnEntitiesInPVS:\n"); fclose(fp); }
	return (*g_engfuncs.pfnEntitiesInPVS)(pplayer);
}
void pfnMakeVectors(const float *rgflVector)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnMakeVectors: rgflVector=%f\n", *rgflVector); fclose(fp); }
	(*g_engfuncs.pfnMakeVectors)(rgflVector);
}
void pfnAngleVectors(const float *rgflVector, float *forward, float *right, float *up)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnAngleVectors:\n"); fclose(fp); }
	(*g_engfuncs.pfnAngleVectors)(rgflVector, forward, right, up);
}
edict_t* pfnCreateEntity(void)
{
	edict_t *pent = (*g_engfuncs.pfnCreateEntity)();
#ifdef _DEBUG
	if (debug_engine)
	{
		fp=fopen(debug_fname,"a");
		if (pent->v.classname != NULL)
			fprintf(fp,"pfnCreateEntity: %p (classname: %s)\n",pent, STRING(pent->v.classname));
		else
			fprintf(fp,"pfnCreateEntity: %p\n",pent);
		fclose(fp);
	}
#endif
	return pent;
}
void pfnRemoveEntity(edict_t* e)
{
#ifdef _DEBUG
	if (debug_engine)
	{
		fp=fopen(debug_fname,"a");
		if (e->v.classname != NULL)
			fprintf(fp,"pfnRemoveEntity: %p (classname: %s)\n",e, STRING(e->v.classname));
		else
			fprintf(fp,"pfnRemoveEntity: %p\n",e);
		if (e->v.model != 0)
			fprintf(fp," model=%s\n", STRING(e->v.model));
		fclose(fp);
	}
#endif
	
	(*g_engfuncs.pfnRemoveEntity)(e);
}
edict_t* pfnCreateNamedEntity(int className)
{
	edict_t *pent = (*g_engfuncs.pfnCreateNamedEntity)(className);
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCreateNamedEntity: edict=%p name=%s\n",pent,STRING(className)); fclose(fp); }
#endif

	// Sort of a hotfix for the crashes of Linux game client on map Charlie. Hopefully better solution can be found. One that doesn't disable the mortar weapons on the map for team Axis.
	if (internals.IsFixParticleManagerCrash() && (strcmp(STRING(className), "monster_mortar") == 0))
	{
		pent->v.flags |= FL_KILLME;
	}
	// Hotfix end

	return pent;
}
void pfnMakeStatic(edict_t *ent)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnMakeStatic:\n"); fclose(fp); }
	(*g_engfuncs.pfnMakeStatic)(ent);
}
int pfnEntIsOnFloor(edict_t *e)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnEntIsOnFloor:\n"); fclose(fp); }
	return (*g_engfuncs.pfnEntIsOnFloor)(e);
}
int pfnDropToFloor(edict_t* e)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDropToFloor:\n"); fclose(fp); }
	return (*g_engfuncs.pfnDropToFloor)(e);
}
int pfnWalkMove(edict_t *ent, float yaw, float dist, int iMode)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWalkMove:\n"); fclose(fp); }
	return (*g_engfuncs.pfnWalkMove)(ent, yaw, dist, iMode);
}
void pfnSetOrigin(edict_t *e, const float *rgflOrigin)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetOrigin: client=%x origin=%.1f\n", e, rgflOrigin); fclose(fp); }
	(*g_engfuncs.pfnSetOrigin)(e, rgflOrigin);
}
void pfnEmitSound(edict_t *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch)
{
	int index;
	//edict_t *pEdict;

	if (gpGlobals->deathmatch)
	{
		if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnEmitSound: edict=%p sound=%s channel=%d volume=%.2f attenuation=%.2f fFlags=%d, pitch=%d (gametime=%.3f)\n",entity,sample,channel,volume,attenuation,fFlags,pitch,gpGlobals->time); fclose(fp); }

		// did the bot hear a grenade that has been just thrown?
		if (strstr(sample, "grenthrow.wav") != NULL)
		{
			if (volume > 0.0f)
			{
				for (index=0; index < MAX_CLIENTS; index++)
				{
					if (bots[index].is_used)  // is this slot used?
					{
						// is it this bot who threw the grenade?
						if (entity == bots[index].pEdict)
						{
							bots[index].BotSpeak(voiceCmd::fire_in_the_hole);
							break;
						}
					}
				}
			}
		}
		else if (strstr(sample, "weaponpickup.wav") != NULL)
		{
			for (index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)
				{
					// does the entity pointer match this bot and is he alive (ie. not respawning)...
					if ((entity == bots[index].pEdict) && (bots[index].pEdict->v.deadflag == DEAD_NO))
					{
						// and did this bot drop the box with extra ammo for teammate previously?
						if (bots[index].IsWeaponStatus(WS_DROPAMMO))
							// then this bot must be picking up his own box with extra ammo now so we must reset the flag in order to make him be able to drop it again
							bots[index].RemoveWeaponStatus(WS_DROPAMMO);

						break;
					}
				}
			}
		}

		/*/
		// is someone bleeding?
		else if (strstr(sample, "bleed.wav") != NULL)
		{
			for (index=0; index < MAX_CLIENTS; index++)
            {
				// is this slot used ie. is this entity a bot?
				if (bots[index].is_used)
				{
					pEdict = bots[index].pEdict;

					// is it this bot who bleeds
					if (entity == pEdict)
					{
						// the bot doesn't bleed any more
						if (volume == 0.0f)
							bots[index].RemoveTask(TASK_BLEEDING);

						// the bot bleeds
						if (volume > 0.0f)
						{
							bots[index].SetTask(TASK_BLEEDING);
						}

						break;
					}

					// we can/should ignore non-bleeding for the rest of this code
					if (volume == 0.0f)
						continue;
				}
				// then this entity must be a human player
				else
				{
					// is it this player who bleeds
					if (entity == clients[index].pEntity)
					{
						if (volume == 0.0f)
							clients[index].SetBleeding(false);

						if (volume > 0.0f)
							clients[index].SetBleeding(true);

						break;
					}
				}
			}
		}

		// skip all muted sounds for the rest of sound messages
		if (volume == 0.0f)
		{
		}

		// is any medic close AND this bot is bleeding?
		else if ((strstr(sample, "voice_medhere.wav") != NULL) || (strstr(sample, "voice_treatyou.wav") != NULL))
		{
			for (index=0; index < MAX_CLIENTS; index++)
            {
				if (bots[index].is_used)  // is this slot used?
				{
					// does this bot bleed and the bot doesn't bandage himself and is NOT in exposed area (eg. close to map goal) and heard the medic calling?
					if (bots[index].IsTask(TASK_BLEEDING) && (bots[index].IsBandagingNow() == false) &&
						(bots[index].IsTask(TASK_IGNORE_ENEMY) == false) && util.CanBotHearThisVoiceMessage(&bots[index], entity, 600.0f))
					{
						// pause for a while to allow the medic to get close
						bots[index].SetPausedTime(RANDOM_FLOAT(2.5f, 5.0f));
					}
				}
			}
		}
		/**/
	}

	(*g_engfuncs.pfnEmitSound)(entity, channel, sample, volume, attenuation, fFlags, pitch);
}
void pfnEmitAmbientSound(edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnEmitAmbientSound:\n"); fclose(fp); }
	
	(*g_engfuncs.pfnEmitAmbientSound)(entity, pos, samp, vol, attenuation, fFlags, pitch);
}
void pfnTraceLine(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceLine:\n"); fclose(fp); }
   (*g_engfuncs.pfnTraceLine)(v1, v2, fNoMonsters, pentToSkip, ptr);
}
void pfnTraceToss(edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceToss:\n"); fclose(fp); }
   (*g_engfuncs.pfnTraceToss)(pent, pentToIgnore, ptr);
}
int pfnTraceMonsterHull(edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceMonsterHull:\n"); fclose(fp); }
   return (*g_engfuncs.pfnTraceMonsterHull)(pEdict, v1, v2, fNoMonsters, pentToSkip, ptr);
}
void pfnTraceHull(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceHull: v1=%f v2=%f fnoMonsters=%d, hullNumber=%d pentToSkip=%x traceresult=%f\n", *v1, *v2, fNoMonsters, hullNumber, pentToSkip, ptr->flFraction); fclose(fp); }
   (*g_engfuncs.pfnTraceHull)(v1, v2, fNoMonsters, hullNumber, pentToSkip, ptr);
}
void pfnTraceModel(const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceModel:\n"); fclose(fp); }
   (*g_engfuncs.pfnTraceModel)(v1, v2, hullNumber, pent, ptr);
}
const char *pfnTraceTexture(edict_t *pTextureEntity, const float *v1, const float *v2 )
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceTexture:\n"); fclose(fp); }
   return (*g_engfuncs.pfnTraceTexture)(pTextureEntity, v1, v2);
}
void pfnTraceSphere(const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTraceSphere:\n"); fclose(fp); }
   (*g_engfuncs.pfnTraceSphere)(v1, v2, fNoMonsters, radius, pentToSkip, ptr);
}
void pfnGetAimVector(edict_t* ent, float speed, float *rgflReturn)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetAimVector:\n"); fclose(fp); }
   (*g_engfuncs.pfnGetAimVector)(ent, speed, rgflReturn);
}
#ifndef NEWSDKAM
void pfnServerCommand(char* str)
#else
void pfnServerCommand(const char* str)
#endif
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnServerCommand: %s\n",str); fclose(fp); }
	(*g_engfuncs.pfnServerCommand)(str);
}
void pfnServerExecute(void)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnServerExecute:\n"); fclose(fp); }
   (*g_engfuncs.pfnServerExecute)();
}
#ifndef NEWSDKAM
void pfnClientCommand(edict_t* pEdict, char* szFmt, ...)
#else
void pfnClientCommand(edict_t* pEdict, const char* szFmt, ...)
#endif
{
	if (debug_engine)
	{
		// print this also to console
		ALERT(at_console, "pfnClientCommand=%s\n",szFmt);
		
		fp=fopen(debug_fname,"a");
		fprintf(fp,"pfnClientCommand=%s (time=%.3f)\n",szFmt,gpGlobals->time);
		fclose(fp);
	}
	
	// new code to test if this finally fix the problem with bot using the say and teamsay cmds
	static char tempFmt[1024];
	va_list argp;
	va_start(argp, szFmt);
	vsprintf(tempFmt, szFmt, argp);
	va_end(argp);
	
	if (!(pEdict->v.flags & FL_FAKECLIENT))
		(*g_engfuncs.pfnClientCommand)(pEdict, tempFmt);
	
	return;
}
void pfnParticleEffect(const float *org, const float *dir, float color, float count)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnParticleEffect: (count=%.2f)\n", count); fclose(fp); }
   (*g_engfuncs.pfnParticleEffect)(org, dir, color, count);
}
#ifndef NEWSDKAM
void pfnLightStyle(int style, char* val)
#else
void pfnLightStyle(int style, const char* val)
#endif
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnLightStyle:\n"); fclose(fp); }
   (*g_engfuncs.pfnLightStyle)(style, val);
}
int pfnDecalIndex(const char* name)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDecalIndex:\n"); fclose(fp); }
	return (*g_engfuncs.pfnDecalIndex)(name);
}
int pfnPointContents(const float *rgflVector)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPointContents:\n"); fclose(fp); }
   return (*g_engfuncs.pfnPointContents)(rgflVector);
}
void pfnMessageBegin(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
{
	if (gpGlobals->deathmatch)
	{
		int index = -1;
		
		if (ed)
		{
			index = util.GetBotIndex(ed);

			// is this message for a bot?
			if (index != -1)
			{
				botMsgFunction = NULL;     // no msg function until known otherwise
				botMsgEndFunction = NULL;  // no msg end function until known otherwise
				botMsgIndex = index;       // index of bot receiving message

				if (msg_type == message_VGUI)
					botMsgFunction = BotClient_VGUI;
				else if (msg_type == message_WeaponList)
					botMsgFunction = BotClient_WeaponList;
				else if (msg_type == message_CurWeapon)
					botMsgFunction = BotClient_CurrentWeapon;
				else if (msg_type == message_AmmoX)
					botMsgFunction = BotClient_AmmoX;
				else if (msg_type == message_AmmoShort)
					botMsgFunction = BotClient_AmmoShort;
				else if (msg_type == message_AmmoPickup)
					botMsgFunction = BotClient_AmmoPickup;
				else if (msg_type == message_WeapPickup)
					botMsgFunction = BotClient_WeaponPickup;
				else if (msg_type == message_ItemPickup)
					botMsgFunction = BotClient_ItemPickup;
				else if (msg_type == message_Health)
					botMsgFunction = BotClient_Health;
				else if (msg_type == message_Damage)
					botMsgFunction = BotClient_Damage;
				else if (msg_type == message_TextMsg)
					botMsgFunction = BotClient_TextMsg;
				else if (msg_type == message_FOV)
					botMsgFunction = BotClient_FOV;
				else if (msg_type == message_ScreenFade)
					botMsgFunction = BotClient_ScreenFade;
				else if (msg_type == message_MOTD)
					botMsgFunction = BotClient_MOTD;
				else if (msg_type == message_Object)
					botMsgFunction = BotClient_Object;
				else if (msg_type == message_ClientAreas)
					botMsgFunction = BotClient_ClientAreas;
				else if (msg_type == message_ReloadDone)
				{
					botMsgFunction = BotClient_ReloadDone;

					// this message is empty, ie. not a single value gets sent, so we have to manually call bot client message right here with "some nothing" or else it won't work at all
					(*botMsgFunction)((void*)NULL, botMsgIndex);
				}
				else if (msg_type == message_HandSignal)
					botMsgFunction = BotClient_HandSignal;
				else if (msg_type == 23)
				{
					botMsgFunction = BotClient_FA_HUDMsg;
				}

			}
		}
		else if (msg_dest == MSG_ALL)
		{
			botMsgFunction = NULL;  // no msg function until known otherwise
			botMsgIndex = -1;       // index of bot receiving message (none)

			if (msg_type == message_DeathMsg)
				botMsgFunction = BotClient_DeathMsg;
			
			else if (msg_type == message_InitObj)
				botMsgFunction = BotClient_InitObj;
			else if (msg_type == message_SetObj)
				botMsgFunction = BotClient_SetObj;
			else if (msg_type == message_PlayersIn)
				botMsgFunction = BotClient_PlayersIn;
			else if (msg_type == message_RoundState)
				botMsgFunction = BotClient_RoundState;
			else if (msg_type == message_CapMsg)
				botMsgFunction = BotClient_CapMsg;
		}

		if (debug_engine)
		{
			//if (msg_type == message_BrokenLeg)
			{
				// prevents spamming the debugging file with this message
				//dont_dump_this_message = true;
			}

			if (!dont_dump_this_message)
				fp = fopen(debug_fname, "a"); fprintf(fp, "pfnMessageBegin: edict=%p dest=%d type=%d (gametime=%.3f)\n", ed, msg_dest, msg_type, gpGlobals->time); fclose(fp);
		}
	}
	
	(*g_engfuncs.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ed);
}
void pfnMessageEnd(void)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine)
		{
			if (!dont_dump_this_message)
			{
				fp = fopen(debug_fname, "a"); fprintf(fp, "pfnMessageEnd:\n"); fclose(fp);
			}
			else
				dont_dump_this_message = false;	// reset "spam message blocker"
		}

		if (botMsgEndFunction)
			(*botMsgEndFunction)(NULL, botMsgIndex);  // NULL indicated msg end
		
		// clear out the bot message function pointers...
		botMsgFunction = NULL;
		botMsgEndFunction = NULL;

		// in DoD the engine message Text Message gets sent with variable amount of values where there is no way to guess their count (well at least no easy yet guaranteed way)
		// so this switch allows us to safely reset the static counter in the function that process this message
		internals.SetNullEngineTextMsgState();
	}
	
	(*g_engfuncs.pfnMessageEnd)();
}
void pfnWriteByte(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteByte: %d\n",iValue); fclose(fp); }
		
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}
	
	(*g_engfuncs.pfnWriteByte)(iValue);
}
void pfnWriteChar(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteChar: %d\n",iValue); fclose(fp); }
		
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}
	
	(*g_engfuncs.pfnWriteChar)(iValue);
}
void pfnWriteShort(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteShort: %d\n",iValue); fclose(fp); }

		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}
	
	(*g_engfuncs.pfnWriteShort)(iValue);
}
void pfnWriteLong(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteLong: %d\n",iValue); fclose(fp); }
		
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}
	
	(*g_engfuncs.pfnWriteLong)(iValue);
}
void pfnWriteAngle(float flValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteAngle: %f\n",flValue); fclose(fp); }
		
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&flValue, botMsgIndex);
	}
	
	(*g_engfuncs.pfnWriteAngle)(flValue);
}
void pfnWriteCoord(float flValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteCoord: %f\n",flValue); fclose(fp); }
		
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&flValue, botMsgIndex);
	}

	(*g_engfuncs.pfnWriteCoord)(flValue);
}
void pfnWriteString(const char *sz)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteString: %s\n",sz); fclose(fp); }
	
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)sz, botMsgIndex);	
	}
	
	(*g_engfuncs.pfnWriteString)(sz);
}
void pfnWriteEntity(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		if (debug_engine && !dont_dump_this_message) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnWriteEntity: %d\n",iValue); fclose(fp); }
		
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}
	
	(*g_engfuncs.pfnWriteEntity)(iValue);
}
void pfnCVarRegister(cvar_t *pCvar)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCVarRegister:\n"); fclose(fp); }
   (*g_engfuncs.pfnCVarRegister)(pCvar);
}
float pfnCVarGetFloat(const char *szVarName)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCVarGetFloat: %s\n",szVarName); fclose(fp); }
   return (*g_engfuncs.pfnCVarGetFloat)(szVarName);
}
const char* pfnCVarGetString(const char *szVarName)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCVarGetString:\n"); fclose(fp); }
   return (*g_engfuncs.pfnCVarGetString)(szVarName);
}
void pfnCVarSetFloat(const char *szVarName, float flValue)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCVarSetFloat:\n"); fclose(fp); }
   (*g_engfuncs.pfnCVarSetFloat)(szVarName, flValue);
}
void pfnCVarSetString(const char *szVarName, const char *szValue)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCVarSetString: varName=%s value=%s\n", szVarName, szValue); fclose(fp); }
	(*g_engfuncs.pfnCVarSetString)(szVarName, szValue);
}

#if !defined ( NEWSDKAM ) && !defined ( NEWSDKVALVE ) && !defined ( NEWSDKMM ) && !defined ( __linux__ )
// original HL SDK v2.3
// if you're getting errors here then go to 'defines.h' and set the correct flag
// matching your HL SDK version
void* pfnPvAllocEntPrivateData(edict_t *pEdict, long cb)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPvAllocEntPrivateData: <%d>\n", cb); fclose(fp); }
   return (*g_engfuncs.pfnPvAllocEntPrivateData)(pEdict, cb);
}
#else
// all other newer HL SDKs and original HL SDK v2.3 on Linux
void* pfnPvAllocEntPrivateData(edict_t* pEdict, int32 cb)
{
	if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnPvAllocEntPrivateData:\n"); fclose(fp); }
	return (*g_engfuncs.pfnPvAllocEntPrivateData)(pEdict, cb);
}
#endif

void* pfnPvEntPrivateData(edict_t *pEdict)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPvEntPrivateData:\n"); fclose(fp); }
	return (*g_engfuncs.pfnPvEntPrivateData)(pEdict);
}
void pfnFreeEntPrivateData(edict_t *pEdict)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnFreeEntPrivateData:\n"); fclose(fp); }
	(*g_engfuncs.pfnFreeEntPrivateData)(pEdict);
}
const char* pfnSzFromIndex(int iString)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSzFromIndex:\n"); fclose(fp); }
   return (*g_engfuncs.pfnSzFromIndex)(iString);
}
int pfnAllocString(const char *szValue)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnAllocString: <%s>\n", szValue); fclose(fp); }
   return (*g_engfuncs.pfnAllocString)(szValue);
}
entvars_t* pfnGetVarsOfEnt(edict_t *pEdict)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetVarsOfEnt:\n"); fclose(fp); }
   return (*g_engfuncs.pfnGetVarsOfEnt)(pEdict);
}
edict_t* pfnPEntityOfEntOffset(int iEntOffset)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPEntityOfEntOffset:\n"); fclose(fp); }
   return (*g_engfuncs.pfnPEntityOfEntOffset)(iEntOffset);
}
int pfnEntOffsetOfPEntity(const edict_t *pEdict)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnEntOffsetOfPEntity: %x\n",pEdict); fclose(fp); }
   return (*g_engfuncs.pfnEntOffsetOfPEntity)(pEdict);
}
int pfnIndexOfEdict(const edict_t *pEdict)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnIndexOfEdict: %x\n",pEdict); fclose(fp); }
   return (*g_engfuncs.pfnIndexOfEdict)(pEdict);
}
edict_t* pfnPEntityOfEntIndex(int iEntIndex)
{
//#ifdef _DEBUG
	//edict_t* pent = (*g_engfuncs.pfnPEntityOfEntIndex)(iEntIndex);
	
	/*/			// gets called really often
	if (debug_engine)
	{
		fp=fopen(debug_fname,"a");
		if (pent != NULL)
		{
			if (pent->v.classname != NULL)
			{
				if (pent->v.netname != NULL)
					fprintf(fp,"pfnPEntityOfEntIndex: %x (classname: %s) (netname: %s)\n",
						pent, STRING(pent->v.classname), STRING(pent->v.netname));
				else
					fprintf(fp,"pfnPEntityOfEntIndex: %x (classname: %s)\n",pent, STRING(pent->v.classname));
			}
			else
				fprintf(fp,"pfnPEntityOfEntIndex: %x\n",pent);
		}
		else
			// index 32 is the last one and is always NULL 
			fprintf(fp,"pfnPEntityOfEntIndex: pent must be NULL see its iEntIndex: %d\n", iEntIndex);
		fclose(fp);
	}
	/**/
	//return pent;
//#else
	return (*g_engfuncs.pfnPEntityOfEntIndex)(iEntIndex);
//#endif
}
edict_t* pfnFindEntityByVars(entvars_t* pvars)
{
	if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnFindEntityByVars:\n"); fclose(fp); }
	return (*g_engfuncs.pfnFindEntityByVars)(pvars);
}
void* pfnGetModelPtr(edict_t* pEdict)
{
	//if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnGetModelPtr: %p\n", pEdict); fclose(fp); }
	return (*g_engfuncs.pfnGetModelPtr)(pEdict);
}


int pfnRegUserMsg(const char* pszName, int iSize)
{
	if(strcmp(pszName, "ScoreInfoLong") == 0)
		pszName = "ScoreInfoL";

	int msg = (*g_engfuncs.pfnRegUserMsg)(pszName, iSize);

	if (gpGlobals->deathmatch)
	{
#ifdef _DEBUG
		if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnRegUserMsg: pszName=%s (iSize=%d) msg=%d\n", pszName, iSize, msg); fclose(fp); }
#endif

		if (strcmp(pszName, "VGUIMenu") == 0)
			message_VGUI = msg;
		else if (strcmp(pszName, "WeaponList") == 0)
			message_WeaponList = msg;
		else if (strcmp(pszName, "CurWeapon") == 0)
			message_CurWeapon = msg;
		else if (strcmp(pszName, "AmmoX") == 0)
			message_AmmoX = msg;
		else if (strcmp(pszName, "AmmoShort") == 0)
			message_AmmoShort = msg;
		else if (strcmp(pszName, "AmmoPickup") == 0)
			message_AmmoPickup = msg;
		else if (strcmp(pszName, "WeapPickup") == 0)
			message_WeapPickup = msg;
		else if (strcmp(pszName, "ItemPickup") == 0)
			message_ItemPickup = msg;
		else if (strcmp(pszName, "Health") == 0)
			message_Health = msg;
		else if (strcmp(pszName, "Damage") == 0)
			message_Damage = msg;
		else if (strcmp(pszName, "DeathMsg") == 0)
			message_DeathMsg = msg;
		else if (strcmp(pszName, "TextMsg") == 0)
			message_TextMsg = msg;
		else if (strcmp(pszName, "SetFOV") == 0)
			message_FOV = msg;
		else if (strcmp(pszName, "ScreenFade") == 0)
			message_ScreenFade = msg;
		else if (strcmp(pszName, "MOTD") == 0)
			message_MOTD = msg;
		else if (strcmp(pszName, "Object") == 0)
			message_Object = msg;
		else if (strcmp(pszName, "ClientAreas") == 0)
			message_ClientAreas = msg;
		else if (strcmp(pszName, "ReloadDone") == 0)
			message_ReloadDone = msg;
		else if (strcmp(pszName, "HandSignal") == 0)
			message_HandSignal = msg;
		else if (strcmp(pszName, "InitObj") == 0)
			message_InitObj = msg;
		else if (strcmp(pszName, "SetObj") == 0)
			message_SetObj = msg;
		else if (strcmp(pszName, "PlayersIn") == 0)
			message_PlayersIn = msg;
		else if (strcmp(pszName, "RoundState") == 0)
			message_RoundState = msg;
		else if (strcmp(pszName, "CapMsg") == 0)
			message_CapMsg = msg;
	}

	return msg;
}
void pfnAnimationAutomove(const edict_t* pEdict, float flTime)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnAnimationAutomove:\n"); fclose(fp); }
   (*g_engfuncs.pfnAnimationAutomove)(pEdict, flTime);
}
void pfnGetBonePosition(const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles )
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetBonePosition:\n"); fclose(fp); }
   (*g_engfuncs.pfnGetBonePosition)(pEdict, iBone, rgflOrigin, rgflAngles);
}

#if !defined ( NEWSDKAM ) && !defined ( NEWSDKVALVE ) && !defined ( NEWSDKMM ) && !defined ( __linux__ )
// original HL SDK v2.3
unsigned long pfnFunctionFromName( const char *pName )
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnFunctionFromName:\n"); fclose(fp); }

   return FUNCTION_FROM_NAME(pName);
}
const char *pfnNameForFunction( unsigned long function )
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnNameForFunction:\n"); fclose(fp); }

   return NAME_FOR_FUNCTION(function);
}
#else
// all other newer HL SDKs and original HL SDK v2.3 on Linux
uint32 pfnFunctionFromName(const char* pName)
{
	if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnFunctionFromName:\n"); fclose(fp); }

	return (*g_engfuncs.pfnFunctionFromName)(pName);
}
const char* pfnNameForFunction(uint32 function)
{
	if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnNameForFunction:\n"); fclose(fp); }

	return (*g_engfuncs.pfnNameForFunction)(function);
}
#endif

void pfnClientPrintf( edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg )
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnClientPrintf: %s\n", szMsg); fclose(fp); }

   // don't print it for nonexistent clients
   if (pEdict == NULL)
	   return;

   (*g_engfuncs.pfnClientPrintf)(pEdict, ptype, szMsg);
}
void pfnServerPrint( const char *szMsg )
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnServerPrint: %s\n",szMsg); fclose(fp); }
   (*g_engfuncs.pfnServerPrint)(szMsg);
}
void pfnGetAttachment(const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles )
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetAttachment: %d\n", iAttachment); fclose(fp); }
   (*g_engfuncs.pfnGetAttachment)(pEdict, iAttachment, rgflOrigin, rgflAngles);
}
void pfnCRC32_Init(CRC32_t *pulCRC)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCRC32_Init:\n"); fclose(fp); }
	(*g_engfuncs.pfnCRC32_Init)(pulCRC);
}
void pfnCRC32_ProcessBuffer(CRC32_t *pulCRC, void *p, int len)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCRC32_ProcessBuffer:\n"); fclose(fp); }
	(*g_engfuncs.pfnCRC32_ProcessBuffer)(pulCRC, p, len);
}
void pfnCRC32_ProcessByte(CRC32_t *pulCRC, unsigned char ch)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCRC32_ProcessByte:\n"); fclose(fp); }
	(*g_engfuncs.pfnCRC32_ProcessByte)(pulCRC, ch);
}
CRC32_t pfnCRC32_Final(CRC32_t pulCRC)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCRC32_Final:\n"); fclose(fp); }
	return (*g_engfuncs.pfnCRC32_Final)(pulCRC);
}

#if !defined ( NEWSDKAM ) && !defined ( NEWSDKVALVE ) && !defined ( NEWSDKMM ) && !defined ( __linux__ )
// original HL SDK v2.3
long pfnRandomLong(long lLow, long lHigh)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnRandomLong: lLow=%d lHigh=%d\n",lLow,lHigh); fclose(fp); }
   return (*g_engfuncs.pfnRandomLong)(lLow, lHigh);
}
#else
// all other newer HL SDKs and original HL SDK v2.3 on Linux
int32 pfnRandomLong(int32 lLow, int32 lHigh)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnRandomLong: lLow=%d lHigh=%d\n",lLow,lHigh); fclose(fp); }
	return (*g_engfuncs.pfnRandomLong)(lLow, lHigh);
}
#endif

float pfnRandomFloat(float flLow, float flHigh)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnRandomFloat:\n"); fclose(fp); }
   return (*g_engfuncs.pfnRandomFloat)(flLow, flHigh);
}
void pfnSetView(const edict_t *pClient, const edict_t *pViewent )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetView: pClient %p pViewent %p\n", pClient, pViewent); fclose(fp); }
#endif

	// NOTE: This seems to finally FIXED the bloody trigger_camera crash on obj_armory
	//		 Although I'm not sure whether this won't affect things like HLTV.

	if (pClient != NULL)
	{
		if (util.GetBotIndex((edict_t *)pClient) != -1)
		{
			//fp=fopen(debug_fname,"a");
			//fprintf(fp,"pfnSetView(): pClient is NOT a real human player. It could be a BOT -> not passing!\n");
			//fclose(fp);

			return;
		}
	}

	(*g_engfuncs.pfnSetView)(pClient, pViewent);
}
float pfnTime( void )
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnTime:\n"); fclose(fp); }
   return (*g_engfuncs.pfnTime)();
}
void pfnCrosshairAngle(const edict_t *pClient, float pitch, float yaw)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCrosshairAngle:\n"); fclose(fp); }
   (*g_engfuncs.pfnCrosshairAngle)(pClient, pitch, yaw);
}
#ifndef NEWSDKAM
byte *pfnLoadFileForMe(char *filename, int *pLength)
#else
byte* pfnLoadFileForMe(const char* filename, int* pLength)
#endif
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnLoadFileForMe: filename=%s\n",filename); fclose(fp); }
   return (*g_engfuncs.pfnLoadFileForMe)(filename, pLength);
}
void pfnFreeFile(void *buffer)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnFreeFile:\n"); fclose(fp); }
   (*g_engfuncs.pfnFreeFile)(buffer);
}
void pfnEndSection(const char *pszSectionName)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnEndSection:\n"); fclose(fp); }
   (*g_engfuncs.pfnEndSection)(pszSectionName);
}
int pfnCompareFileTime(char *filename1, char *filename2, int *iCompare)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCompareFileTime:\n"); fclose(fp); }
   return (*g_engfuncs.pfnCompareFileTime)(filename1, filename2, iCompare);
}
void pfnGetGameDir(char *szGetGameDir)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetGameDir:\n"); fclose(fp); }
   (*g_engfuncs.pfnGetGameDir)(szGetGameDir);
}
void pfnCvar_RegisterVariable(cvar_t *variable)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCvar_RegisterVariable:\n"); fclose(fp); }
   (*g_engfuncs.pfnCvar_RegisterVariable)(variable);
}
void pfnFadeClientVolume(const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnFadeClientVolume:\n"); fclose(fp); }
   (*g_engfuncs.pfnFadeClientVolume)(pEdict, fadePercent, fadeOutSeconds, holdTime, fadeInSeconds);
}
void pfnSetClientMaxspeed(const edict_t* pEdict, float fNewMaxspeed)
{
#ifdef _DEBUG
	if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnSetClientMaxspeed: edict=%p %f (globTime %.3f)\n", pEdict, fNewMaxspeed, gpGlobals->time); fclose(fp); }


	//@@@@@@@@@@@@@22
	//ALERT(at_console, "pfnSetClientMaxspeed: edict=%x speed=%.1f\n",pEdict,fNewMaxspeed);
#endif


	if (gpGlobals->deathmatch)
	{
		int index;

		index = util.GetBotIndex((edict_t*)pEdict);

		// is this edict a bot?
		if (index != -1)
		{
			// update current max speed for this bot
			bots[index].SetMaxSpeed(fNewMaxspeed);


#ifdef _DEBUG
			if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "pfnSetClientMaxspeed: edict=%p %f\n", pEdict, fNewMaxspeed); fclose(fp); }


			//@@@@@@@@@@@@@
			ALERT(at_console, "pfnSetClientMaxspeed: edict=%x speed=%.1f\n",pEdict,fNewMaxspeed);
#endif


			/*/
			// check for max speed changes only when bot has already joined the game (ie. ignore any SetClientMaxSpeed() that is called while picking team and class)
			if (bots[index].IsBotFlag(BF_NOT_JOINED_GAME) == false)
			{
				// we must set this flag for correct respawn, because Firearms mod doesn't kill the player when the "round" ended
				bots[index].SetBotFlag(BF_RESPAWN_AT_ROUND_END);
			}
			/**/
		}

		/*/
		index = 0;
		while ((index < MAX_CLIENTS) && (clients[index].pEntity != pEdict))
			index++;

		if (index < MAX_CLIENTS)
		{
			// store the time when this client was "out of the game"
			clients[index].SetMaxSpeedTime(gpGlobals->time);
		}
		/**/
	}
	
	(*g_engfuncs.pfnSetClientMaxspeed)(pEdict, fNewMaxspeed);
}
edict_t * pfnCreateFakeClient(const char *netname)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCreateFakeClient:\n"); fclose(fp); }
   return (*g_engfuncs.pfnCreateFakeClient)(netname);
}
void pfnRunPlayerMove(edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec )
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnRunPlayerMove:\n"); fclose(fp); }
   (*g_engfuncs.pfnRunPlayerMove)(fakeclient, viewangles, forwardmove, sidemove, upmove, buttons, impulse, msec);
}
int pfnNumberOfEntities(void)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnNumberOfEntities:\n"); fclose(fp); }
   return (*g_engfuncs.pfnNumberOfEntities)();
}
char* pfnGetInfoKeyBuffer(edict_t *e)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetInfoKeyBuffer:\n"); fclose(fp); }
   return (*g_engfuncs.pfnGetInfoKeyBuffer)(e);
}
#ifndef NEWSDKAM
char* pfnInfoKeyValue(char *infobuffer, char *key)
#else
char* pfnInfoKeyValue(char* infobuffer, const char* key)
#endif
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnInfoKeyValue: %s %s\n",infobuffer,key); fclose(fp); }
   return (*g_engfuncs.pfnInfoKeyValue)(infobuffer, key);
}
#ifndef NEWSDKAM
void pfnSetKeyValue(char *infobuffer, char *key, char *value)
#else
void pfnSetKeyValue(char* infobuffer, const char* key, const char* value)
#endif
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetKeyValue: %s %s\n",key,value); fclose(fp); }
   (*g_engfuncs.pfnSetKeyValue)(infobuffer, key, value);
}
#ifndef NEWSDKAM
void pfnSetClientKeyValue(int clientIndex, char *infobuffer, char *key, char *value)
#else
void pfnSetClientKeyValue(int clientIndex, char* infobuffer, const char* key, const char* value)
#endif
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetClientKeyValue: %s %s\n",key,value); fclose(fp); }
   (*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, key, value);
}
#ifndef NEWSDKAM
int pfnIsMapValid(char *filename)
#else
int pfnIsMapValid(const char* filename)
#endif
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnIsMapValid:\n"); fclose(fp); }
   return (*g_engfuncs.pfnIsMapValid)(filename);
}
void pfnStaticDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex )
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnStaticDecal:\n"); fclose(fp); }
   (*g_engfuncs.pfnStaticDecal)(origin, decalIndex, entityIndex, modelIndex);
}
#ifndef NEWSDKAM
int pfnPrecacheGeneric(char* s)
#else
int pfnPrecacheGeneric(const char* s)
#endif
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPrecacheGeneric: %s\n",s); fclose(fp); }
   return (*g_engfuncs.pfnPrecacheGeneric)(s);
}
int pfnGetPlayerUserId(edict_t *e )
{
#ifdef _DEBUG
   if (gpGlobals->deathmatch)
   {
      if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetPlayerUserId: %p\n",e); fclose(fp); }
   }
#endif

   return (*g_engfuncs.pfnGetPlayerUserId)(e);
}
const char *pfnGetPlayerAuthId (edict_t *e)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetPlayerAuthId: %p\n",e); fclose(fp); }
#endif
   
   if (e->v.flags & FL_FAKECLIENT)
	   return "BOT";

   return (*g_engfuncs.pfnGetPlayerAuthId)(e);
}
void pfnBuildSoundMsg(edict_t *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnBuildSoundMsg:\n"); fclose(fp); }

	// test though this shouldn't cause any harm
	if (entity->v.flags & FL_FAKECLIENT)
		return;

	(*g_engfuncs.pfnBuildSoundMsg)(entity, channel, sample, volume, attenuation, fFlags, pitch, msg_dest, msg_type, pOrigin, ed);
}
int pfnIsDedicatedServer(void)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnIsDedicatedServer:\n"); fclose(fp); }
   return (*g_engfuncs.pfnIsDedicatedServer)();
}
cvar_t* pfnCVarGetPointer(const char *szVarName)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCVarGetPointer: %s\n",szVarName); fclose(fp); }
   return (*g_engfuncs.pfnCVarGetPointer)(szVarName);
}
unsigned int pfnGetPlayerWONId(edict_t *e)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetPlayerWONId: %p\n",e); fclose(fp); }
#endif

   if (e->v.flags & FL_FAKECLIENT)
	   return 0;

   return (*g_engfuncs.pfnGetPlayerWONId)(e);
}


// new stuff for SDK 2.0

void pfnInfo_RemoveKey(char *s, const char *key)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnInfo_RemoveKey:\n"); fclose(fp); }
   (*g_engfuncs.pfnInfo_RemoveKey)(s, key);
}
const char *pfnGetPhysicsKeyValue(const edict_t *pClient, const char *key)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetPhysicsKeyValue: %p %s\n", pClient, key); fclose(fp); }
#endif
   return (*g_engfuncs.pfnGetPhysicsKeyValue)(pClient, key);
}
void pfnSetPhysicsKeyValue(const edict_t *pClient, const char *key, const char *value)
{
#ifdef _DEBUG
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetPhysicsKeyValue: client=%p key=%s value=%s (time=%.3f)\n", pClient, key, value, gpGlobals->time); fclose(fp); }
#endif
   (*g_engfuncs.pfnSetPhysicsKeyValue)(pClient, key, value);
}
const char *pfnGetPhysicsInfoString(const edict_t *pClient)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetPhysicsInfoString: %x\n", pClient); fclose(fp); }
   return (*g_engfuncs.pfnGetPhysicsInfoString)(pClient);
}
unsigned short pfnPrecacheEvent(int type, const char *psz)
{
   if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPrecacheEvent: %s\n", psz); fclose(fp); }
   return (*g_engfuncs.pfnPrecacheEvent)(type, psz);
}

void DoRecoil(bot_t* pBot, unsigned short eventindex) {
	float factor = 1.f;
	float pitch = 0.f;
	float yaw = 2.f;
	switch(eventindex) {
	case  1: //thompson
	case 17: //greasegun
		pitch = 2.15f;
		factor = 0.5375f;
		break;

	case  2: //m1carbine
	case  5: //luger
	case  7: //colt
	case 24: //webley
		pitch = 1.4f;
		factor = 0.35f;
		break;

	case  3: //garand
	case  9: //kar
	case 20: //enfield
		pitch = 8.f;
		factor = 2.f;
		break;

	case  4: //scopedkar
	case 21: //scopedenfield   
		pitch = 6.f;
		factor = 1.5f;
		break;

	case  6: //springfield
		pitch = 5.6f;
		factor = 1.4f;
		break;

	case 10: //mp44
		pitch = 5.f;
		factor = 1.25f;
		break;

	case 11: //mp40
	case 22: //sten
		pitch = 2.2f;
		factor = 0.55f;
		break;

	case 12: //mg42
	case 14: //mg34
	case 15: //30cal
		pitch = 20.f;
		factor = 5.f;
		break;

	case 16: //bar
	case 23: //bren
		pitch = 6.72f;
		factor = 1.3f;
		break;

	case 18: //fg42
		pitch = 5.3f;
		factor = 1.325f;
		break;

	case 19: //k43
		pitch = 7.f;
		factor = 1.75f;
		break;
	default:
		return;
	}
	
	auto pPlayer = pBot->pEdict;
	if(pPlayer->v.iuser3 == 2 || pPlayer->v.vuser1.x == 2)
		return;
	yaw = factor;
	factor = pPlayer->v.iuser3 ? 0.25f : 0.5f;
	pitch *= factor;
	yaw *= factor;

	yaw *= RANDOM_FLOAT(0.8f, 1.1f);
	if(RANDOM_LONG(0, 1))
		yaw = -yaw;

	pPlayer->v.v_angle.x -= pitch;
	pPlayer->v.v_angle.y += yaw;
	
	pBot->AddRecoil(pitch);
}

void pfnPlaybackEvent(int flags, const edict_t *pInvoker, unsigned short eventindex, float delay,
   float *origin, float *angles, float fparam1,float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnPlaybackEvent: flags=%d invoker=%p index=%d fparam1=%.2f fparam2=%.2f iparam1=%d iparam2=%d bparam1=%d bparam2=%d\n",
		flags, pInvoker, eventindex, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2); fclose(fp); }
#endif

	// is it the use of a voice command? (29 is allies, 30 is axis)
	if ((eventindex == 29) || (eventindex == 30))
	{
		int index;
		edict_t* pEdict;

		// is it a Hold this position command?
		if (iparam1 == 2)
		{
			for (index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)	// is this slot used?
				{
					pEdict = bots[index].pEdict;

					// skip "yourself"
					if (pEdict == pInvoker)
						continue;

					// is this bot NOT doing the waiting duties yet AND can hear this command?
					if ((bots[index].IsWaitTime() == false) && util.CanBotHearThisVoiceMessage(&bots[index], (edict_t*)pInvoker, TEAMMATE_SEARCH_RADIUS))
					{
						// gunners should try to utilize the bipod
						if (bots[index].IsBehaviour(MGUNNER))
						{
							//TODO: Check for standing bipod spot in vicinity to use it

							if (CanDeployBipod(pEdict))
								BotUseBipod(&bots[index], true, "PlaybackEvent()|HoldPositionCmd -> DEPLOY bipod for the waiting");
							else
							{
								if (RANDOM_LONG(1, 100) < (90 - (bots[index].GetBotSkill() * 5)))
									bots[index].SetStance(GOTO_PRONE, "PlaybackEvent()|HoldPositionCmd -> GOTO prone for the waiting");
							}
						}
						else
						{
							if (RANDOM_LONG(1, 100) > 50)
								bots[index].SetStance(GOTO_CROUCH, "PlaybackEvent()|HoldPositionCmd -> GOTO crouch for the waiting");
						}

						// set the wait time and make the bot hold the position even when he notices an enemy, also make him keep assigned stance while waiting
						bots[index].SetWaitTime(bots[index].GenerateHoldPositionTime());
						bots[index].SetTask(TASK_DONTMOVEINCOMBAT);
						// also make him keep assigned stance while waiting by giving him a task to ignore all waypoints
						bots[index].SetTask(TASK_IGNOREWPTNAV);
						
						bots[index].BotSpeak(voiceCmd::yes_sir);

						// stop following the team leader to be able to hold the position
						bots[index].pTeamLeader = NULL;
					}
				}
			}
		}

		// does someone call for backup?
		else if (iparam1 == 12)
		{
			for (index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)
				{
					if (bots[index].pEdict == pInvoker)
						continue;

					// has this bot already a team leader then ignore the call
					if (bots[index].pTeamLeader != NULL)
						continue;

					// is this bot a leader AND does the call come from another bot? then ignore it
					if (util.CanBeFireTeamLeader(&bots[index]) && (util.GetBotIndex((edict_t*)pInvoker) != NO_VAL))
						continue;

					// can this bot hear the call?
					if (util.CanBotHearThisVoiceMessage(&bots[index], (edict_t*)pInvoker, TEAMMATE_SEARCH_RADIUS * 1.25f))	// originally was 400 in FA, this will ensure similar radius where default botskill means exactly 300 units
					{
						// follow this team leader only if he doesn't have too many team members
						if (util.CountFireTeamMembers((edict_t*)pInvoker) < FIRETEAM_SIZE - 1)
						{
							bots[index].pTeamLeader = (edict_t*)pInvoker;
							// set see team leader time to know that this bot has just joined this team
							bots[index].SetSeeTeamLeaderTime();

							bots[index].BotSpeak(voiceCmd::yes_sir);

							// bot must reset aims in order to allow watching surrounding right away when the team leader stops just a few seconds after "taking" this bot
							bots[index].ResetAims("engine.cpp | voice needbackup -> going to follow team leader");
						}
						// say no, this team leader already has enough team members
						else
						{
							bots[index].BotSpeak(voiceCmd::negative);
						}
					}
				}
			}
		}

		// is it a Go go go command?
		else if (iparam1 == 19)
		{
			for (index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)
				{
					if (bots[index].pEdict == pInvoker)
						continue;

					// ignore it while being in capture area trying to help with the capturing or waiting for a teammate
					if (bots[index].IsTask(TASK_PARACHUTE) || bots[index].IsSubTask(ST_PARACHUTE_USED))
					{
						bots[index].BotSpeak(voiceCmd::negative);
						continue;
					}

					// is this bot doing the waiting duties somewhere AND can hear this command?
					if (bots[index].IsWaitTime() && util.CanBotHearThisVoiceMessage(&bots[index], (edict_t*)pInvoker, TEAMMATE_SEARCH_RADIUS))
					{
						// then break the waiting
						bots[index].SetWaitTime(0.0f);

						bots[index].BotSpeak(voiceCmd::yes_sir);
					}
				}
			}
		}

		// is it a I need ammo command?
		else if (iparam1 == 25)
		{
			for (index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)
				{
					if (bots[index].pEdict == pInvoker)
						continue;

					if (util.CanBotHearThisVoiceMessage(&bots[index], (edict_t*)pInvoker, TEAMMATE_SEARCH_RADIUS))
					{
						// did bot already drop the extra ammo?
						if (bots[index].IsWeaponStatus(WS_DROPAMMO))
							bots[index].BotSpeak(voiceCmd::negative);
						else
						{
							// doesn't bot already deal with any other game entity?
							if (bots[index].HasNoGEnt())
							{
								// set the task to drop the box with extra ammo
								bots[index].SetTask(TASK_MEDEVAC);
								// remember the direction to which the bot should "throw" the ammo
								bots[index].SetPointerToGEnt((edict_t*)pInvoker);
								// bot needs to turn towards a game entity (the teammate in this case) so don't look for any aim waypoint around
								bots[index].SetTask(TASK_IGNOREAIMWPTS);
								// make bot wait for a while in order to give the caller some time to pickup the ammo
								bots[index].SetWaitTime(RANDOM_FLOAT(2.0f, 5.0f));
								// let the caller know
								bots[index].BotSpeak(voiceCmd::yes_sir);

								// no point to look for another bot to drop another ammo
								break;
							}
							else
								bots[index].BotSpeak(voiceCmd::negative);
						}
					}
				}
			}
		}

		// is it a Use the Bazooka command?
		else if (iparam1 == 26)
		{
			for (index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)
				{
					if (bots[index].pEdict == pInvoker)
						continue;

					// is this bot an anti-armor specialist AND can he hear this command?
					if (bots[index].IsBehaviour(AASPEC) && util.CanBotHearThisVoiceMessage(&bots[index], (edict_t*)pInvoker, TEAMMATE_SEARCH_RADIUS))
					{
						if (bots[index].IsNoAmmoForMainWeapon())
						{
							bots[index].BotSpeak(voiceCmd::negative, 0.7f);
							continue;
						}

						bots[index].ResetAims("PlaybackEvent()|UseBazookaCmd");

						bool found_breakable_object = false;
						Vector v_src, v_des;
						TraceResult tr;

						// first see if the teammate that used this command is pointing to some breakable object...
						UTIL_MakeVectors(pInvoker->v.v_angle);
						// so let's send a traceline from teammate's eyes straight forward up to possible target object
						v_src = pInvoker->v.origin + pInvoker->v.view_ofs;
						v_des = v_src + gpGlobals->v_forward * (EXTENDED_SEARCH_RADIUS * 2.0f);
						UTIL_TraceLine(v_src, v_des, dont_ignore_monsters, pInvoker->v.pContainingEntity, &tr);

						// did the traceline hit a breakable object that can be destroyed by this bot OR is there a breakable object next to the spot this teammate points to AND
						// is this target object far enough (ie. bot must be farther than half of the min safe distance for the launcher)?
						if (((util.IsEntityName(tr.pHit, "func_breakable") && (util.NotBreakableByGunfire(tr.pHit) == false) && (util.NotBreakableByThisTeam(tr.pHit, bots[index].GetBotTeam()) == false)) ||
							util.CheckForBreakableAround(&bots[index], STANDARD_SEARCH_RADIUS, tr.vecEndPos)) && bots[index].IsInSafeDistanceToShoot(tr.vecEndPos))
						{
							found_breakable_object = true;
							// tell the bot that his teammate points to possible target
							bots[index].SetSubTask(ST_MEDEVAC_F);
							// store away the hit point so that the bot can aim at it
							bots[index].SetPositionOfPointInSpace(tr.vecEndPos);
						}

						// teammate doesn't point to any breakable object so see if there's any breakable object around the bot that isn't too close
						if ((bots[index].IsSubTask(ST_MEDEVAC_F) == false) && util.CheckForBreakableAround(&bots[index], EXTENDED_SEARCH_RADIUS) &&
							bots[index].IsInSafeDistanceToShoot(bots[index].GetDistanceToGEnt()))
							found_breakable_object = true;

						// did we find anything?
						if (found_breakable_object)
						{
							// then set some wait time to destroy the object
							bots[index].SetWaitTime(3.0f);
							bots[index].SetSubTask(ST_MEDEVAC_ST);
							bots[index].SetSubTask(ST_MEDEVAC_H);
							bots[index].SetSubTask(ST_USEEYESORIGIN);
							bots[index].SetTask(TASK_IGNOREAIMWPTS);
							bots[index].BotSpeak(voiceCmd::yes_sir, 0.7f);
						}
						else
							bots[index].BotSpeak(voiceCmd::negative, 0.7f);
					}
				}
			}
		}
	}

	if(pInvoker)
		for(int iBot = 0; iBot < MAX_CLIENTS; iBot++)
			if(bots[iBot].is_used && bots[iBot].pEdict == pInvoker)
				DoRecoil(&bots[iBot], eventindex);
	
	(*g_engfuncs.pfnPlaybackEvent)(flags, pInvoker, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
}
unsigned char *pfnSetFatPVS(float *org)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetFatPVS:\n"); fclose(fp); }
   return (*g_engfuncs.pfnSetFatPVS)(org);
}
unsigned char *pfnSetFatPAS(float *org)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetFatPAS: org=%f\n", *org); fclose(fp); }
   return (*g_engfuncs.pfnSetFatPAS)(org);
}
int pfnCheckVisibility(const edict_t *entity, unsigned char *pset)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCheckVisibility: entity=%x\n", entity); fclose(fp); }
   return (*g_engfuncs.pfnCheckVisibility)(entity, pset);
}
void pfnDeltaSetField(struct delta_s *pFields, const char *fieldname)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDeltaSetField:\n"); fclose(fp); }
   (*g_engfuncs.pfnDeltaSetField)(pFields, fieldname);
}
void pfnDeltaUnsetField(struct delta_s *pFields, const char *fieldname)
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDeltaUnsetField:\n"); fclose(fp); }
   (*g_engfuncs.pfnDeltaUnsetField)(pFields, fieldname);
}
#ifndef NEWSDKAM
void pfnDeltaAddEncoder(char *name, void (*conditionalencode)( struct delta_s *pFields, const unsigned char *from, const unsigned char *to))
#else
void pfnDeltaAddEncoder(const char* name, void (*conditionalencode)(struct delta_s* pFields, const unsigned char* from, const unsigned char* to))
#endif
{
   //if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDeltaAddEncoder:\n"); fclose(fp); }
   (*g_engfuncs.pfnDeltaAddEncoder)(name, conditionalencode);
}
int pfnGetCurrentPlayer(void)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetCurrentPlayer:\n"); fclose(fp); }
	return (*g_engfuncs.pfnGetCurrentPlayer)();
}
int pfnCanSkipPlayer(const edict_t *player)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCanSkipPlayer: player: %x\n", player); fclose(fp); }
	return (*g_engfuncs.pfnCanSkipPlayer)(player);
}
int pfnDeltaFindField(struct delta_s *pFields, const char *fieldname)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDeltaFindField:\n"); fclose(fp); }
	return (*g_engfuncs.pfnDeltaFindField)(pFields, fieldname);
}
void pfnDeltaSetFieldByIndex(struct delta_s *pFields, int fieldNumber)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDeltaSetFieldByIndex:\n"); fclose(fp); }
	(*g_engfuncs.pfnDeltaSetFieldByIndex)(pFields, fieldNumber);
}
void pfnDeltaUnsetFieldByIndex(struct delta_s *pFields, int fieldNumber)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnDeltaUnsetFieldByIndex: filednumber=%d\n", fieldNumber); fclose(fp); }
	(*g_engfuncs.pfnDeltaUnsetFieldByIndex)(pFields, fieldNumber);
}
void pfnSetGroupMask(int mask, int op)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnSetGroupMask:\n"); fclose(fp); }
	(*g_engfuncs.pfnSetGroupMask)(mask, op);
}
int pfnCreateInstancedBaseline(int classname, struct entity_state_s *baseline)
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCreateInstancedBaseline:\n"); fclose(fp); }
	return (*g_engfuncs.pfnCreateInstancedBaseline)(classname, baseline);
}
#ifndef NEWSDKAM
void pfnCvar_DirectSet(struct cvar_s *var, char *value)
#else
void pfnCvar_DirectSet(struct cvar_s* var, const char* value)
#endif
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnCvar_DirectSet: value=%s\n", value); fclose(fp); }
	(*g_engfuncs.pfnCvar_DirectSet)(var, value);
}
void pfnForceUnmodified(FORCE_TYPE type, float *mins, float *maxs, const char *filename)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnForceUnmodified:\n"); fclose(fp); }
	(*g_engfuncs.pfnForceUnmodified)(type, mins, maxs, filename);
}
void pfnGetPlayerStats(const edict_t *pClient, int *ping, int *packet_loss)
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnGetPlayerStats:\n"); fclose(fp); }
	(*g_engfuncs.pfnGetPlayerStats)(pClient, ping, packet_loss);
}
#ifndef NEWSDKAM
void pfnAddServerCommand(char *cmd_name, void (*function)(void))
#else
void pfnAddServerCommand(const char* cmd_name, void (*function)(void))
#endif
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"pfnAddServerCommand: %s %p\n",cmd_name,function); fclose(fp); }
#endif
	(*g_engfuncs.pfnAddServerCommand)(cmd_name, function);
}

