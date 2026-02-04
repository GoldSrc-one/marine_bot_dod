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
// bot_func.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BOT_FUNC_H
#define BOT_FUNC_H

class utils_t
{
public:
	void InitPrivateVars(void);
	void TraceLineIgnoringPlayers(const Vector& vecStart, const Vector& vecEnd, TraceResult* ptr);
	edict_t* FindEntityInSphere(edict_t* pentStart, const Vector& vecCenter, float flRadius);
	edict_t* FindEntityInSphere(edict_t* pCallerEdict, const char* ent_name);
	edict_t* FindEntityByClassname(edict_t* pentStart, const char* szName);
	edict_t* FindEntityByTargetname(edict_t* pentStart, const char* szName);
	int GetTeam(edict_t* pEntity);
	int GetBotIndex(edict_t* pEdict);
	bool IsAlive(edict_t* pEdict);
	bool IsEdictProne(edict_t* pEdict);
	bool IsEdictCrouched(edict_t* pEdict);
	bool IsEntityName(edict_t* pEntity, const char* entity_classname);
	bool IsDoorEntity(edict_t* pEntity);
	bool IsDoorOpen(edict_t* pEntity, bool is_crouched);
	bool IsOfficialWaypoints(const char* waypointer);
	bool IsNewerVersion(void);
	bool IsOldVersion(void);
	bool IsInViewCone(Vector* pOrigin, edict_t* pEdict);
	bool IsInNarrowViewCone(Vector* pOrigin, edict_t* pEdict, float cone = 0.8);
	int InFieldOfView(edict_t* pEdict, const Vector& dest);
	bool IsVisible(const Vector& vecOrigin, edict_t* pEdict);
	bool IsVisible(const Vector& vecOrigin, edict_t* pEdict, bool ignore_water_restriction);
	int IsPlayerVisible(const Vector& vecOrigin, const Vector& vecLookerOrigin, edict_t* pEdict);
	int IsPlayerVisible(const Vector& vecOrigin, edict_t* pEdict);
	bool IsWaypointVisible(Vector& vecOrigin, edict_t* pEdict);
	bool IsWaypointVisibleThroughDoor(Vector& vecOrigin, Vector other_vecOrigin, edict_t* pEdict);
	Vector GetGunPosition(edict_t* pEdict);
	Vector VecBModelOrigin(edict_t* pEdict);
	Vector VecAbsoluteOrigin(edict_t* pEdict);
	Vector GetEntityOrigin(edict_t* pEdict);
	void MarineBotFileName(char* filename, const char* arg1, const char* arg2);
	bool TraceObjectsSides(edict_t* pEdict, int side, const Vector& obj_origin);
	void HandSignal(edict_t* pEdict, const char* signal);
	void Voice(edict_t* pEdict, voiceCmd command);
	//void Say(edict_t* pEdict, const char* message);//		NOT USED
	void TeamSay(edict_t* pEdict, const char* message);
	bool CanBotHearThisVoiceMessage(bot_t* pBot, edict_t* pInvoker, float range);
	bool CanBotSeeThisHandSignal(bot_t* pBot, edict_t* pInvoker, float range = TEAMMATE_SEARCH_RADIUS * 5.0f);
	int FindBotByName(const char* name_string);
	bool KickBot(int which_one);
	bool KillBot(int which_one);
	int CountPlayers(int team);
	int GetTeamOnePlayerCount(void);
	int GetTeamTwoPlayerCount(void);
	int TeamsBalanceCheck(void);
	int ExecuteTeamsBalance(void);
	int ChangeBotSkillLevel(bool by_one, int skill_level);
	int ChangeAimSkillLevel(int skill_level);
	bool IsPointReachable(const Vector& vecStart, const Vector& vecDestination, bool ignore_players);
	bool IsEntityNearby(bot_t* pBot, const char* entity_classname, float radius);
	bool IsCaptureAreaNearby(bot_t* pBot, float radius);
	bool IsTeammateNearby(bot_t* pBot, float radius, int num_of_teamates = 1, bool ignore_fireteam = false);
	bool IsExplosivesChargeNearby(bot_t* pBot, float radius, const Vector custom_start_point = g_vecZero);
	bool CanBeFireTeamLeader(bot_t* pBot);
	int CountFireTeamMembers(edict_t* pLeader);
	bool CanBotCaptureTheArea(bot_t* pBot, bool ignore_teammate_requirement = false);
	bool NotBreakableByGunfire(edict_t* pEdict);
	bool IsEntityBreakableByExplosivesOnly(edict_t* pEntity);
	bool NotBreakableByThisTeam(edict_t* pEntity, int team);
	bool CheckForwardForBreakable(bot_t* pBot, bool check_vicinity = false);
	bool CheckForBreakableAround(bot_t* pBot, float radius, const Vector custom_start_point = g_vecZero);
	bool CheckForClaymoreOnlySDObjectAround(bot_t* pBot);
	bool CheckForUsablesAround(bot_t* pBot);
	void HumanizeTheName(const char* original_name, char* name);
	void RemoveIllegalCharsFromBuffer(char* buffer);
	void RemoveNewlineCharsFromBufferEnd(char* buffer);
	void RemoveTagsFromBotname(const char* botname, char* stripped_name);

	void ShowMenu(edict_t* pEdict, int slots, int displaytime, bool needmore, char* pText);
	void PrintAvailableWeapons(bot_t* pBot);
	char* StripWeaponName(char* name_buffer);
	char* PrintWeaponName(int weaponID);
	char* ConvertUsedWeaponToString(bot_t* pBot);

	void HighlightTrace(Vector v_source, Vector v_dest, edict_t* pEdict);
	void DebugInFile(const char* msg);
	void DebugDev(const char* msg, int wpt = -100, int path = -100);
	void DebugDev(const char* msg, const char* string, int wpt = -100, int path = -100);
	void DumpEdictToFile(edict_t* pEdict);

private:
	void CountPlayersInBothTeams(void);

	int teamOne_player_count;
	int teamTwo_player_count;
	float teams_player_count_update_time;
};

extern utils_t util;

//prototypes of bot functions
void BotNameArraysInit(void);
bool BotNamesInit(const char* bot_names_filename);
char* GetNationFromBotName(const char* bot_name);
bool FreeBotName(const char* bot_name, int bot_team);
bool BotCreate( edict_t *pPlayer, const char *arg1, const char *arg2, const char *arg3, const char *arg4, const char *arg5 );
void BotFixIdealPitch( edict_t *pEdict );
float BotChangePitch( bot_t *pBot, float speed );
void BotFixIdealYaw( edict_t *pEdict );
float BotChangeYaw( bot_t *pBot, float speed );
//bool BotFindWaypoint( bot_t *pBot );
bool BotHeadTowardWaypoint( bot_t *pBot );
//void BotOnLadder( bot_t *pBot, float moved_distance );
void BotUnderWater( bot_t *pBot );
//void BotUseLift( bot_t *pBot, float moved_distance );


bool BotStuckInCorner( bot_t *pBot );
void BotTurnAtWall( bot_t *pBot, TraceResult *tr );
bool BotCantMoveForward( bot_t *pBot, TraceResult *tr );
bool IsForwardBlocked(bot_t *pBot);
bool IsDeathFall(edict_t* pEdict);
bool BotCanJumpUp(bot_t* pBot);
bool BotCanDuckJumpUp(bot_t* pBot);
bool BotCanDuckJumpInto(bot_t* pBot);
bool BotCanDuckUnder(bot_t* pBot);
bool BotCheckWallOnLeft( bot_t *pBot );
bool BotCheckWallOnRight( bot_t *pBot );

bool InitWeaponsForThisMod(void);
bool BotWeaponArraysInit(const char* weapon_definitions_filename);
bool BotTargetOffsetsArrayInit(const char* target_offsets_filename);
bool IsAssaultRifle(int weapon, int specific_class);
bool IsSniperRifle(int weapon, int specific_class);
bool IsMachinegun(int weapon);
bool IsLMG(int weapon, int specific_class);
bool IsSMG(int weapon);
bool IsRPG(int weapon);
bool IsHandgun(int weapon);
bool IsMelee(int weapon);
bool IsGrenade(int weapon);
bool IsPrimary(int weapon);
bool IsBipodWeapon(int weapon, int specific_class);
bool IsLimitedReloadMachinegun(int weapon);
bool IsWeaponWithOptics(int weapon, bool has_necessary_condition);
void DefineReloadTimeForCurrentWeapon(bot_t* pBot);
void BotShootAtEnemy( bot_t *pBot );
Vector BotBodyTarget(bot_t* pBot);
void BotFireWeaponOutOfCombat(bot_t* pBot);
bool BotSelectWeaponToDestroyThisBreakable(bot_t* pBot, bool check_distance = false);
bool IsInStandingBipodSpot(edict_t* pEdict);
bool CanDeployBipod(edict_t* pEdict);
void BotUseBipod(bot_t* pBot, bool forced_call, const char* loc = NULL);
float GetBipodHandlingTime(int weapon, int specific_class, bool deploying);
void BotSwitchGrenadeLauncherFireMode(bot_t* pBot, const char* loc = NULL);
bool IsGrenadeLauncherReadyToFire(edict_t* pEdict, int weapon);
bool IsGrenadeLauncherOffShoulder(edict_t* pEdict);


bool BotFindWaypoint(bot_t *pBot, bool ladder);
bool BotHandleLadder(bot_t *pBot, float moved_distance);
bool BotCantStrafeLeft(edict_t *pEdict);
bool BotCantStrafeRight(edict_t *pEdict);
bool BotFollowTeamLeader(bot_t *pBot);


// bot_start.cpp prototypes
void BotSetWeaponsUsage(bot_t* pBot);


// util.cpp functions
void ClientPrint(edict_t* pEntity, int msg_dest, const char* msg_name, const char* string);
void ClientPrint( edict_t *pEdict, int msg_dest, const char *msg_name);
//void UTIL_SayText( const char *pText, edict_t *pEdict );				// NOT USED
//void UTIL_HostSay( edict_t *pEntity, int teamonly, char *message );	// NOT USED
//bot_t *UTIL_GetBotPointer(edict_t *pEdict);							// NOT USED
//int ForwardTrace(const Vector &vecOrigin, edict_t *pEdict);	// NOT USED
//int UTIL_GetIDFromName(const char *weapon_name);		// NOT USED
//float UTIL_IsInRange(edict_t *pEdict, int wpt_index);	// NOT USED
//int UTIL_GetLadderDir(bot_t *pBot);					// NOT USED


// waypoint.cpp prototypes
void ResetTriggersOnRoundEnd(void);
bool IsMatchingTriggerMessage(char *the_text);

// mb_hud prototypes
static unsigned short FixedUnsigned16( float value, float scale );
static short FixedSigned16( float value, float scale );

void FullCustHudMessage(edict_t *pEntity, const char *msg_name, int channel, float pos_x, float pos_y, int gfx, Vector color1, Vector color2, int brightness, float fade_in, float fade_out, float duration);
void StdHudMessage(edict_t *pEntity, const char *msg_name, int gfx, int time);
void StdHudMessageToAll(const char *msg_name, int gfx, int time);
void CustHudMessage(edict_t *pEntity, const char *msg_name, Vector color1, Vector color2, int gfx, int time);
void CustHudMessageToAll(const char *msg_name, Vector color1, Vector color2, int gfx, int time);
void DisplayMsg(edict_t *pEntity, const char *msg);
void CustDisplayMsg(edict_t *pEntity, const char *msg, float x_pos, float y_pos, float time);

#endif // BOT_FUNC_H

