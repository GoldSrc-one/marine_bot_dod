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
// bot_manager.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BOT_MANAGER_H
#define BOT_MANAGER_H

// Marine Bot unified error message system flags
#define UEMS_WELCOME1			(1<<0)		// HUD welcome - reporting for duty & version msg (or simple one sentence text to Dedicated server console)
#define UEMS_WELCOME2			(1<<1)		// HUD welcome - MB webpage msg
#define UEMS_WELCOME3			(1<<2)		// HUD welcome - waypoint authors message
#define UEMS_ALLWSENT			(1<<3)		// all HUD welcome messages have been displayed
#define UEMS_ER_CFG				(1<<4)		// missing configuration file
#define UEMS_ER_WPNDEF			(1<<5)		// invalid or missing weapon definitions file
#define UEMS_ER_WPNLNK			(1<<6)		// internal link of weapon names and game weapon IDs failed
#define UEMS_WARN_CFG_CMDS		(1<<7)		// syntax error in commands (key binding) section in .cfg file
#define UEMS_WARN_CFG_CVARS		(1<<8)		// syntax error in CVAR section in .cfg file
#define UEMS_WARN_CFG_MISSCC	(1<<9)		// missing custom class header in .cfg file
#define UEMS_WARN_CFG_NULLCC	(1<<10)		// empty custom class in .cfg file
#define UEMS_WARN_CFG_REC		(1<<11)		// syntax error in custom recruiting section in .cfg file
#define UEMS_WARN_BNAME			(1<<12)		// invalid or missing file with bot names
#define UEMS_WARN_PTH			(1<<13)		// invalid or missing waypoint paths file
#define UEMS_WARN_WPT			(1<<14)		// invalid or missing waypoint file
#define UEMS_WARN_TARGOFS		(1<<15)		// invalid or missing target body offsets file

// Error messages used to inform the user about detected issues
const char uems_header[] = "WARNING - MarineBot detected an error:\n";	// there needs to be the newline (else when the message followed on the same line then there were crashes with certain messages, I'll have to check the source of these crashes in FullCustHudMessage() one day)
const char uems_cfg[] = "missing \"marine.cfg\" configuration file!\nBots may join or play incorrectly";
const char uems_wpndef[] = "syntax error or missing weapon definitions file!\nBots may not shoot";
const char uems_wpnlink[] = "linking internal weapon names with game weapon IDs!\nBots will not shoot";
const char uems_cfgcmds[] = "syntax error in configuration file!\nThe \"commands\" may not work";
const char uems_cfgcvars[] = "syntax error in the configuration file!\nUsing default CVAR setting";
const char uems_cfgmisscc[] = "missing custom class/classes in the configuration file!\nSome bots may not join the game";
const char uems_cfgnullcc[] = "empty custom class/classes in the configuration file!\nSome bots may not play well due to missing gear";
const char uems_cfgrec[] = "syntax error in the configuration file!\n\"Custom recruiting\" could fail";
const char uems_bname[] = "missing file with bot names!\nBots will use the same default name \"marine\"";
const char uems_wpt[] = "invalid or missing waypoint file for this map!\nBots may play incorrectly";
const char uems_pth[] = "invalid or missing path file for this map!\nBots may play incorrectly";
const char uems_targofs[] = "syntax error or missing target body offsets file!\nUsing hard-coded defaults for aiming";

// the max number of clients in the game
#define MAX_CLIENTS			32
constexpr int MAX_CAPTUREPOINTS = 12;
constexpr int CAPTUREPOINTS_ERROR_VAL = -1;


void UTIL_StringFromBuffer(char* string, const char* buffer, int left_bracket_char, int right_bracket_char);


class client_t
{
public:
	client_t();
	inline bool IsHuman() { return client_is_human; }
	inline void SetHuman(bool flag) { client_is_human = flag; }
	inline bool IsBleeding() { return client_bleeds; }
	inline void SetBleeding(bool flag) { client_bleeds = flag; }
	inline void SetMaxSpeedTime(float time) { max_speed_time = time; }
	inline float GetMaxSpeedTime(void) { return max_speed_time; }
	inline int add_human() { return ++humans_num; }
	inline int add_bot() { return ++bots_num; }
	inline int substr_human() { return --humans_num; }
	inline int substr_bot() { return --bots_num; }
	inline int HumanCount() { return humans_num; }
	inline int BotCount() { return bots_num; }
	inline int ClientCount() { return (bots_num + humans_num); }
	edict_t* pEntity;		// pEntity

private:
	bool client_is_human;			// not a fakeclient ie. not a bot
	bool client_bleeds;
	float max_speed_time;			// to check if round ended or if it is only a single death

	// more client and bot globals can be stored here
	// for example current possition for all players (bot and human)

	static int humans_num;
	static int bots_num;
};

extern client_t clients[MAX_CLIENTS];

/*
* used to manage desired bot counts on game server (dedicated server as well as listen server)
*/
class botmanager_t
{
public:
	botmanager_t();
	inline void SetTeamsBalanceNeeded(bool newVal) { teams_balance_needed = newVal; }
	inline bool IsTeamsBalanceNeeded(void) { return teams_balance_needed; }
	inline void ResetTeamsBalanceNeeded(void) { teams_balance_needed = false; }
	inline void SetOverrideTeamsBalance(bool newVal) { override_teams_balance = newVal; }
	inline bool IsOverrideTeamsBalance(void) { return override_teams_balance; }
	inline void ResetOverrideTeamsBalance(void) { override_teams_balance = false; }
	inline void SetTimeOfTeamsBalanceCheck(float newVal) { time_of_teams_balance_check = newVal; }
	inline float GetTimeOfTeamsBalanceCheck(void) { return time_of_teams_balance_check; }
	inline void ResetTimeOfTeamsBalanceCheck(void) { time_of_teams_balance_check = 0.0f; }
	inline void SetTeamsBalanceValue(int newVal) { teams_balance_value = newVal; }
	inline int GetTeamsBalanceValue(void) { return teams_balance_value; }
	inline void ResetTeamsBalanceValue(void) { teams_balance_value = 0; }
	inline void SetListeServerFilling(bool newVal) { listenserver_filling = newVal; }
	inline bool IsListenServerFilling(void) { return listenserver_filling; }
	inline void ResetListenServerFilling(void) { listenserver_filling = false; }
	inline void SetBotCheckTime(float newVal) { bot_check_time = newVal; }
	inline float GetBotCheckTime(void) { return bot_check_time; }
	inline void ResetBotCheckTime(void) { bot_check_time = 0.0f; }
	inline void SetBotsToBeAdded(int newVal) { bots_to_be_added = newVal; }
	inline int GetBotsToBeAdded(void) { return bots_to_be_added; }
	inline void ResetBotsToBeAdded(void) { bots_to_be_added = 0; }
	inline void DecreaseBotsToBeAdded(void) { bots_to_be_added--; }

private:
	bool teams_balance_needed;			// teams don't have same counts of players so bots must change team to make the counts equal - by kota@
	bool override_teams_balance;		// stops teams balancing when reinforcements of one team reached zero
	float time_of_teams_balance_check;	// the time of last teams balance check
	int teams_balance_value;		// keeps teams balanced: <=0-teams balanced, >0&&<100-blue balance value, >100-red balance value
	bool listenserver_filling;		// allows automatically add bots to ListenServer
	float bot_check_time;			// time to next bot addition
	int bots_to_be_added;			// amount of bots we need to add (ie. "arg filling")
};

extern botmanager_t botmanager;

/*
* class of variables used to debug bot behaviour
*/
class botdebugger_t
{
public:
	botdebugger_t();
	inline void SetObserverMode(bool newVal) { observer_mode = newVal; }
	inline bool IsObserverMode(void) { return observer_mode; }
	inline void ResetObserverMode(void) { observer_mode = false; }
	inline void SetFreezeMode(bool newVal) { freeze_mode = newVal; }
	inline bool IsFreezeMode(void) { return freeze_mode; }
	inline void ResetFreezeMode(void) { freeze_mode = false; }
	inline void SetDontShoot(bool newVal) { dont_shoot = newVal; }
	inline bool IsDontShoot(void) { return dont_shoot; }
	inline void ResetDontShoot(void) { dont_shoot = false; }
	inline void SetDontShootFirearm(bool newVal) { dont_shoot_firearm = newVal; }
	inline bool IsDontShootFirearm(void) { return dont_shoot_firearm; }
	inline void ResetDontShootFirearm(void) { dont_shoot_firearm = false; }
	inline void SetIgnoreAll(bool newVal) { ignore_all = newVal; }
	inline bool IsIgnoreAll(void) { return ignore_all; }
	inline void ResetIgnoreAll(void) { ignore_all = false; }
	inline void SetDebugAims(bool newVal) { debug_aims = newVal; }
	inline bool IsDebugAims(void) { return debug_aims; }
	inline void ResetDebugAims(void) { debug_aims = false; }
	inline void SetDebugActions(bool newVal) { debug_actions = newVal; }
	inline bool IsDebugActions(void) { return debug_actions; }
	inline void ResetDebugActions(void) { debug_actions = false; }
	inline void SetDebugCross(bool newVal) { debug_cross = newVal; }
	inline bool IsDebugCross(void) { return debug_cross; }
	inline void ResetDebugCross(void) { debug_cross = false; }
	inline void SetDebugPaths(bool newVal) { debug_paths = newVal; }
	inline bool IsDebugPaths(void) { return debug_paths; }
	inline void ResetDebugPaths(void) { debug_paths = false; }
	inline void SetDebugStance(bool newVal) { debug_stance = newVal; }
	inline bool IsDebugStance(void) { return debug_stance; }
	inline void ResetDebugStance(void) { debug_stance = false; }
	inline void SetDebugStuck(bool newVal) { debug_stuck = newVal; }
	inline bool IsDebugStuck(void) { return debug_stuck; }
	inline void ResetDebugStuck(void) { debug_stuck = false; }
	inline void SetDebugWaypoints(bool newVal) { debug_waypoints = newVal; }
	inline bool IsDebugWaypoints(void) { return debug_waypoints; }
	inline void ResetDebugWaypoints(void) { debug_waypoints = false; }
	inline void SetDebugWeapons(bool newVal, int debugging_level = 0) { debug_weapons = newVal; debug_weapons_level = debugging_level; }
	inline bool IsDebugWeapons(int debugging_level = 0) { return (debug_weapons && (debug_weapons_level >= debugging_level)); }
	inline void ResetDebugWeapons(void) { debug_weapons = false; debug_weapons_level = 0; }

private:
	bool observer_mode;		// makes the player "invisible" for bots, but they will still see each other, but they will still see each other and act accordingly
	bool freeze_mode;		// bots don't move and do any actions
	bool dont_shoot;		// bots will NOT start firing at enemy, but all combat functions will still be run
	bool dont_shoot_firearm;// bots will NOT start firing at enemy from any firearm, but grenades and knife will be used normally
	bool ignore_all;		// bots will ignore all - they won't seek enemies so no combat actions, but they will move and do other actions
	bool debug_aims;		// allows listen server console printing about targeting current and available aim waypoints
	bool debug_actions;		// console printing about current action (eg. parachute use, ammobox or button usage) and regular checks like a check for ammunition reserves for example
	bool debug_cross;		// console printing about cross behaviour
	bool debug_paths;		// console printing when the bot is following a path
	bool debug_stance;		// console printing about bot stance changes
	bool debug_stuck;		// console printing informing about current unstuck action
	bool debug_waypoints;	// console printing about current waypoint the bot heads towards (when bot doesn't follow any path)
	bool debug_weapons;		// console printing about weapons (eg. which are available, which is currently used, in combat usage, reloading etc.)
	int debug_weapons_level;	// determines the level of printed info i.e. the higher the value the more messages gets printed
};

extern botdebugger_t botdebugger;

/*
* class of variables that are available to be set externally in .cfg file
*/
class externals_t
{
public:
	externals_t();
	void ResetOnMapChange(void);
	inline void SetIsLogging(bool newVal) { is_logging = newVal; }
	inline bool GetIsLogging(void) { return is_logging; }
	inline void ResetIsLogging(void) { is_logging = false; }	// will set it to deafult value
	inline void SetRandomSkill(bool newVal) { random_skill = newVal; }
	inline bool GetRandomSkill(void) { return random_skill; }
	inline void ResetRandomSkill(void) { random_skill = false; }
	inline void SetSpawnSkill(int newVal) { spawn_skill = newVal; }
	inline int GetSpawnSkill(void) { return spawn_skill; }
	inline void ResetSpawnSkill(void) { spawn_skill = 3; }
	inline void SetReactionTime(float newVal) { reaction_time = newVal; }
	inline float GetReactionTime(void) { return reaction_time; }
	inline void ResetReactionTime(void) { reaction_time = 0.5f; }
	inline void SetBalanceTime(float newVal) { auto_balance_time = newVal; }
	inline float GetBalanceTime(void) { return auto_balance_time; }
	inline void ResetBalanceTime(void) { auto_balance_time = 30.0f; }
	inline void SetMinBots(int newVal) { min_bots = newVal; }
	inline int GetMinBots(void) { return min_bots; }
	inline void ResetMinBots(void) { min_bots = 2; }
	inline void SetMaxBots(int newVal) { max_bots = newVal; }
	inline int GetMaxBots(void) { return max_bots; }
	inline void ResetMaxBots(void) { max_bots = 6; }
	inline void SetInfoTime(float newVal) { info_time = newVal; }
	inline float GetInfoTime(void) { return info_time; }
	inline void ResetInfoTime(void) { info_time = 150.0f; }
	inline void SetPresentationTime(float newVal) { presentation_time = newVal; }
	inline float GetPresentationTime(void) { return presentation_time; }
	inline void ResetPresentationTime(void) { presentation_time = 210.0f; }
	inline void SetHUDTextLineLength(int newVal) { hudtext_line_length = newVal; }
	inline int GetHUDTextLineLength(void) { return hudtext_line_length; }
	inline void ResetHUDTextLineLength(void) { hudtext_line_length = 80; }
	inline void SetDontSpeak(bool newVal) { dont_speak = newVal; }
	inline bool GetDontSpeak(void) { return dont_speak; }
	inline void ResetDontSpeak(void) { dont_speak = false; }
	inline void SetDontChat(bool newVal) { dont_chat = newVal; }
	inline bool GetDontChat(void) { return dont_chat; }
	inline void ResetDontChat(void) { dont_chat = false; }
	inline void SetDontChatToBots(bool newVal) { dont_chattobots = newVal; }
	inline bool GetDontChatToBots(void) { return dont_chattobots; }
	inline void ResetDontChatToBots(void) { dont_chattobots = false; }
	inline void SetRichNames(bool newVal) { rich_names = newVal; }
	inline bool GetRichNames(void) { return rich_names; }
	inline void ResetRichNames(void) { rich_names = true; }
	inline void SetCustomHeadshotPercentage(int newVal) { custom_headshot_precentage = newVal; }
	inline int GetCustomHeadshotPercentage(void) { return custom_headshot_precentage; }
	inline void ResetCustomHeadshotPercentage(void) { custom_headshot_precentage = -1; }
	inline void SetGrenadeUsePercentage(int newVal) { grenade_use_precentage = newVal; }
	inline int GetGrenadeUsePercentage(void) { return grenade_use_precentage; }
	inline void ResetGrenadeUsePercentage(void) { grenade_use_precentage = 25; }
	inline void SetAlternativeStartPositions(bool newVal) { alternative_start_positions = newVal; }
	inline bool IsAlternativeStartPositions(void) { return alternative_start_positions; }
	inline void ResetAlternativeStartPositions(void) { alternative_start_positions = false; }
	
	inline void SetModifiedCaptureAreas(bool newVal) { modified_capture_areas = newVal; }
	inline bool IsModifiedCaptureAreas(void) { return modified_capture_areas; }
	inline void ResetModifiedCaptureAreas(void) { modified_capture_areas = false; }

	// These two are still public, they should go private, but I don't know if they are going to be used at all, because the code behind these doesn't work

	bool  be_samurai;			// will allow bots to commit suicide in certain situations (for example when there are no reins and bots are "stuck" at opposite team spawn area, ie. neither team can win the game)
	float harakiri_time;		// the delay bots have to finish the game using standard ways (ie. killing the other team and so on) before starting to commit suicides

private:
	bool  is_logging;		// will allow logging MB events into default HL log file
	bool  random_skill;		// do we use default skill or randomly generated skill
	int   spawn_skill;		// default skill when there's no skill specified in recruit command
	float reaction_time;	// applied the first time the bot sees an enemy
	float auto_balance_time;// the time between two team balance tests
	int   min_bots;			// the minimal number of bots on DS (won't be kicked when clients join)
	int   max_bots;			// the maximal number of bots on DS
	float info_time;		// the time for printing various info/summary to DS console
	float presentation_time;// the time between sending two presentation messages
	int   hudtext_line_length;	// maximal length of one line of the HUD text that MB will try to maintain when displaying info about waypoints or paths
	bool  dont_speak;		// the bot will or won't use Voice commands
	bool  dont_chat;		// the bot will or won't use say or say_team commands
	bool  dont_chattobots;	// will limit the text chats to human clients only when this is TRUE (chat related to other bots will be muted)
	bool  rich_names;		// will allow '[MB]' sign being a part of a bot name
	int   custom_headshot_precentage;	// will override hardcoded chance to aim for a headshot
	int   grenade_use_precentage;	// determines how often will the bot try to use the grenade against the enemy
	bool  alternative_start_positions;	// gets set when there is an external file with alternative player start positions for this map
	
	bool  modified_capture_areas;		// gets set when there is an external file with modified capture areas data for this map
};

extern externals_t externals;

/*
* class of global variables that can be set only by using console command (ie. not via .cfg file) and some other internal general purpose variables
*/
class internals_t
{
public:
	internals_t();
	void ResetOnMapChange(void);
	inline void SetIsEnemyDistanceLimit(bool newVal) { is_enemy_distance_limit = newVal; }
	inline bool IsEnemyDistanceLimit(void) { return is_enemy_distance_limit; }
	inline void ResetIsEnemyDistanceLimit(void) { is_enemy_distance_limit = false; }
	inline void SetEnemyDistanceLimit(float newVal) { enemy_distance_limit = (float)newVal; }
	inline float GetEnemyDistanceLimit(void) { return enemy_distance_limit; }
	inline void ResetEnemyDistanceLimit(void) { enemy_distance_limit = 7500.0f; }
	inline void SetChangeStartPositions(bool newVal) { change_start_positions = newVal; }
	inline bool IsChangeStartPosition(void) { return change_start_positions; }
	inline void ResetChangeStartPositions(void) { change_start_positions = true; }// don't add this to reset on map change, keep it as a sort of static variable that the user changes at will
	inline void SetChangeCaptureAreas(bool newVal) { change_capture_areas = newVal; }
	inline bool IsChangeCaptureAreas(void) { return change_capture_areas; }
	inline void ResetChangeCaptureAreas(void) { change_capture_areas = true; }	// don't add this to reset on map change, same as above

	inline void SetHUDMessageTime(float newVal) { hud_messsage_time = newVal; }
	inline float GetHUDMessageTime(void) { return hud_messsage_time; }
	inline void ResetHUDMessageTime(void) { hud_messsage_time = 0.0f; }
	inline void SetTeamPlay(float newVal) { is_team_play = newVal; }
	inline float GetTeamPlay(void) { return is_team_play; }
	inline void ResetTeamPlay(void) { is_team_play = 0.0f; }
	inline void SetTeamPlayChecked(bool newVal) { teamplay_checked = newVal; }
	inline bool IsTeamPlayChecked(void) { return teamplay_checked; }
	inline void ResetTeamPlayChecked(void) { teamplay_checked = false; }
	inline void SetMeleeOnlyMode(bool newVal) { is_melee_only = newVal; }
	inline bool IsMeleeOnlyMode(void) { return is_melee_only; }
	inline void ResetMeleeOnlyMode(void) { is_melee_only = false; }
	
	inline void SetIsCustomWaypoints(bool newVal) { is_custom_waypoints = newVal; }
	inline bool IsCustomWaypoints(void) { return is_custom_waypoints; }
	inline void ResetIsCustomWaypoints(void) { is_custom_waypoints = false; }
	inline void SetWaypoitsAutoSave(bool newVal) { waypoints_autosave = newVal; }
	inline bool IsWaypointsAutoSave(void) { return waypoints_autosave; }
	inline void ResetWaypointsAutoSave(void) { waypoints_autosave = false; }
	inline void SetCustomDefaultWaypointRange(float newVal) { custom_default_waypoint_range = newVal; }
	inline float GetCustomDefaultWaypointRange(void) { return custom_default_waypoint_range; }
	inline void ResetCustomDefaultWaypointRange(void) { custom_default_waypoint_range = 50.0f; }
	inline void SetPathToContinue(int newVal) { path_to_continue = newVal; }
	inline int GetPathToContinue(void) { return path_to_continue; }
	inline void ResetPathToContinue(void) { path_to_continue = -1; }
	inline bool IsPathToContinue(void) { return (path_to_continue != -1); }
	inline void SetIsWaypointConversionUnfinished(void) { is_waypoint_conversion_unfinished = true; }
	inline bool IsWaypointConversionUnfinished(void) { return is_waypoint_conversion_unfinished; }
	inline void ResetIsWaypointConversionUnfinished(void) { is_waypoint_conversion_unfinished = false; }
	inline void SetUpdateWaypointDataTime(float newVal) { update_waypoint_data_time = newVal; }
	inline float GetUpdateWaypointDataTime(void) { return update_waypoint_data_time; }
	inline void ResetUpdateWaypointDataTime(void) { update_waypoint_data_time = 0.0f; }

	inline void SetMBFolderName(char* newName) { strncpy(mb_folder_name, newName, sizeof(mb_folder_name)); }
	inline char* GetMBFolderName(void) { return mb_folder_name; }
	inline void ResetMBFolderName(void) { strcpy(mb_folder_name, "marine_bot"); }
	inline void SetInternalMessage(const char* newMessage) { strncpy(internal_message, newMessage, sizeof(internal_message)); }
	inline char* GetInternalMessage(void) { return internal_message; }
	inline bool IsInternalMessage(void) { return (internal_message[0] != 0); }
	inline void ResetInternalMessage(void) { internal_message[0] = '\0'; }
	inline void SetOverrideClientPrint(void) { override_clientprint = true; }
	inline bool IsOverrideClientPrint(void) { return override_clientprint; }
	inline void ResetOverrideClientPrint(void) { override_clientprint = false; }
	inline void SetNullEngineTextMsgState(void) { null_engine_text_msg_state = true; }
	inline bool IsNullEngineTextMsgState(void) { return null_engine_text_msg_state; }
	inline void ResetNullEngineTextMsgState(void) { null_engine_text_msg_state = false; }

	inline void SetRoundState(int newVal) { round_state = newVal; }
	inline int GetRoundState(void) { return round_state; }
	inline void ResetRoundState(void) { round_state = 1; }
	inline void SetIsBritishTeam(int newVal) { is_british_team = newVal; }
	inline bool IsBritishTeam(void) { return (is_british_team == 1); }
	inline void ResetIsBritishTeam(void) { is_british_team = 0; }
	inline void SetAmerNamesCount(int newVal) { american_names_count = newVal; }
	inline int GetAmerNamesCount(void) { return american_names_count; }
	inline void ResetAmerNamesCount(void) { american_names_count = 0; }
	inline void SetBritNamesCount(int newVal) { british_names_count = newVal; }
	inline int GetBritNamesCount(void) { return british_names_count; }
	inline void ResetBritNamesCount(void) { british_names_count = 0; }
	inline void SetGerNamesCount(int newVal) { german_names_count = newVal; }
	inline int GetGerNamesCount(void) { return german_names_count; }
	inline void ResetGerNamesCount(void) { german_names_count = 0; }
	inline void SetMapGoalBasedOnExplosives(bool newVal) { is_map_goal_based_on_explosives = newVal; }
	inline bool IsMapGoalBasedOnExplosives(void) { return is_map_goal_based_on_explosives; }
	inline void ResetMapGoalBasedOnExplosives(void) { is_map_goal_based_on_explosives = false; }
	inline void SetCheckTriggerCapMessage(bool newVal) { check_trigger_capture_message = newVal; }
	inline bool IsCheckTriggerCapMessage(void) { return check_trigger_capture_message; }
	inline void ResetCheckTriggerCapMessage(void) { check_trigger_capture_message = false; }
	inline void SetBuildCaptureAreasFile(bool newVal) { build_capture_areas_file = newVal; }
	inline bool IsBuildCaptureAreasFile(void) { return build_capture_areas_file; }
	inline void ResetBuildCaptureAreasFile(void) { build_capture_areas_file = false; }	// don't add this one to Reset On Map Change else it would NOT work!
	inline void SetIsFixParticleManagerCrash(bool newVal) { is_fix_particleman_crash = newVal; }
	inline bool IsFixParticleManagerCrash(void) { return is_fix_particleman_crash; }
	inline void ResetIsFixParticleManagerCrash(void) { is_fix_particleman_crash = false; }

private:
	bool is_enemy_distance_limit;	// do we limit the view distance?, useful on maps where the bot can see enemy through skybox - ps_island
	float enemy_distance_limit;		// used to limit the view distance, bots won't see/attack enemies that are farther than this number
	bool change_start_positions;	// do we want to use the alternative player start positions defined in external files
	bool change_capture_areas;		// do we want to use the alternative data for the capture areas defined in external files

	float hud_messsage_time;		// to prevent overloading when displaying waypointing info on HUD
	float is_team_play;
	bool teamplay_checked;
	bool is_melee_only;

	bool is_custom_waypoints;		// allows loading custom waypoints (read from different folder)
	bool waypoints_autosave;		// allows waypoints automatic save
	float custom_default_waypoint_range;	// allows user defined default waypoint range for newly added waypoints
	int path_to_continue;			// index of path that is currently edited
	bool is_waypoint_conversion_unfinished;	// allows calling additional functions after map is fully loaded to finalize the waypoint conversion
	float update_waypoint_data_time;		// allows updating waypoints in real-time based on latest game events or changes made by the waypoint creator

	char mb_folder_name[16];		// MB root folder name - allows using a custom folder name (e.g. 'marinebot')
	char internal_message[256];		// used to print a message to client console in cases when client print function cannot be used eg. while processing another engine message
	bool override_clientprint;		// used to temporarily disable the engine client print function ie. not to call its engine message, useful when another engine message is being processed
	bool null_engine_text_msg_state;// used to deal with variable amount of values/lines for the engine message TextMsg

	int round_state;				// the state of current round (running, which team won etc.)
	int is_british_team;			// determines whether the allied team are british soldiers (allows using british bot names)
	int american_names_count;		// number of american names used
	int british_names_count;
	int german_names_count;
	bool is_map_goal_based_on_explosives;	// assigned when at least one of map goals requires explosives charge (dod object in general) - it's needed in the navigation system, to set right path/waypoint values
	bool check_trigger_capture_message;		// allows displaying the message MB builds based on data gathered from DoD engine CapMsg, useful for waypoint creator to utilize the triggers
	bool build_capture_areas_file;		// allows creation of the external .cfg file with capture areas data for the map while it loads
	bool is_fix_particleman_crash;		// allows using the hotfix for particle manager crash on Linux client
};

extern internals_t internals;

/*
* class of error and warning messages used to inform the user about detected issues
* this is used for all error messages that are same for both the console as well as the HUD
* the HUD welcome messages are included too
*/
class unified_error_messages_system_t
{
public:
	unified_error_messages_system_t();
	inline bool IsErrorCode(int msg_flag) { return (messages_bitmap & msg_flag); }
	void AddErrorCode(int msg_flag);
	void DeleteErrorCode(int msg_flag);
	inline void ResetErrorCodes(void) { messages_bitmap = 0; }
	inline bool IsInCopyOfErCodes(int msg_flag) { return (mbitmap_copy & msg_flag); }
	inline bool IsCopyOfErCodesEmpty(void) { return (mbitmap_copy == 0); }
	inline void AddToCopyOfErCodes(int msg_flag) { mbitmap_copy = mbitmap_copy | msg_flag; }
	void DeleteFromCopyOfErCodes(int msg_flag);
	void MakeCopyOfErCodes(void);
	inline void ResetCopyOfErCodes(void) { mbitmap_copy = 0; }
	inline void SetMessageTime(float newVal) { message_time = (float)newVal; }
	inline float GetMessageTime(void) { return message_time; }
	inline void ResetMessageTime(void) { message_time = 0.0f; }
	inline void ResetHistoryOfLastAddedErCode(void) { last_added_message_flag = 0; }
	inline char* GetError(void) { return error_message; }
	inline void SetError(const char* er_msg) { snprintf(error_message, sizeof(error_message), "%s", er_msg); }
	inline void ResetError(void) { error_message[0] = '\0'; }
	inline char* GetWarning(void) { return warning_message; }
	inline void SetWarning(const char* wrn_msg) { snprintf(warning_message, sizeof(warning_message), "%s", wrn_msg); }
	inline void ResetWarning(void) { warning_message[0] = '\0'; }
	void PrepareErrorAndWarning(int msg_flag = 0);
	bool GetHUDErrorMessage(char* message);

	bool IsAnyErrorMessage(void);
	bool IsAnyWarningMessage(void);
	void ResetMessageSystem(void);
	void SetNextWelcome(void);

private:
	int messages_bitmap;			// a bit map of the message error codes
	int mbitmap_copy;				// copy of the messages bitmap
	float message_time;				// time the message was/will be displayed (if we are using the fancy colored message system at the top of the screen) or printed to the console
	int last_added_message_flag;	// holds the last flag added to message bitmap
	char error_message[128];
	char warning_message[128];
};

extern unified_error_messages_system_t errormsgs;

class development_tools_t
{
#ifdef DEBUG

private:
	bool specific_bot_debugging;		// do we need to debug one specific bot?
	edict_t* pointer_to_specific_bot;	// pointer to a single bot we want to debug

	enum tracelinecolor_t { tlc_default = 0, tlc_teambased, tlc_blue, tlc_green };
	typedef tracelinecolor_t tlColor;
	bool display_tracelines;
	bool override_display_tl;
	int tl_beam_duration;
	tlColor tl_beam_color;

#endif // DEBUG

public:
	development_tools_t();

#ifdef DEBUG

	inline void SetSpecificBotDebugging(bool newVal) { specific_bot_debugging = newVal; }
	inline bool IsSpecificBotDebugging(void) { return specific_bot_debugging; }
	inline void ResetSpecificBotDebugging(void) { specific_bot_debugging = false; }
	inline void SetPointerToSpecificBot(edict_t* pEdict) { pointer_to_specific_bot = pEdict; }
	inline edict_t* GetPointerToSpecificBot(void) { return pointer_to_specific_bot; }
	inline void ResetPointerToSpecificBot(void) { pointer_to_specific_bot = NULL; }
	inline bool IsBotDebugging(edict_t* pEdict) { return (specific_bot_debugging && (pointer_to_specific_bot == pEdict)); }
	inline bool IsCommandValidForThisBot(edict_t* pEdict) { return ((specific_bot_debugging == false) || (specific_bot_debugging && (pointer_to_specific_bot == pEdict))); }

	inline void SetDisplayTracelines(bool newVal) { display_tracelines = newVal; }
	inline bool IsDisplayTracelines(void) { return display_tracelines; }
	inline void SetOverrideDisplayTL(void) { override_display_tl = true; }
	inline bool IsNotOverrideDisplayTL(void) { return (override_display_tl == false); }
	inline void ResetOverrideDisplayTL(void) { override_display_tl = false; }
	inline void SetTLBeamDuration(int newVal) { tl_beam_duration = newVal; }
	inline int GetTLBeamDuration(void) { return tl_beam_duration; }
	void SetTLBeamColor(const char* newColor);
	Vector GetTLBeamColor(bool ignore_default_beam_color = false);
	inline void ResetTLBeamColor(void) { tl_beam_color = tlc_default; }

#else
	inline bool IsBotDebugging(edict_t* pEdict) { return false; }

#endif // DEBUG

};

extern development_tools_t devTool;


/*
* class for standard control points a.k.a. flags
*/
class control_point_t
{
public:
	control_point_t();
	void ResetArray(void);
	int FindPointInArray(edict_t* pEntity);
	int FindPointByObjListIndex(int searched_point_index);
	int FindPointByLinkName(const char* name);
	void AddNewPoint(edict_t* pEntity);
	void SetPointName(edict_t* pEntity, const char* name);
	const char* GetPointName(int array_index);
	void SetPointLinkName(edict_t* pEntity, const char* name); // ie. control point 'targetname' and capture area 'target' create the link between these two entities (it must be the same string)
	const char* GetPointLinkName(int array_index);
	void SetPointObjListIndex(edict_t* pEntity, int value);
	void SetPointObjListIndex(int array_index, int value);
	int GetPointObjListIndex(edict_t* pEntity);
	int GetPointObjListIndex(int array_index);
	void SetOwnedByTeam(edict_t* pEntity, int team_id);
	void SetOwnedByTeam(int array_index, int team_id);
	int GetOwnedByTeam(edict_t* pEntity);
	int GetOwnedByTeam(int array_index);
	void SetTeamOneAllowedToCapture(edict_t* pEntity, int value);
	void SetTeamOneAllowedToCapture(int array_index, bool value);
	bool GetTeamOneAllowedToCapture(int array_index);
	void SetTeamTwoAllowedToCapture(edict_t* pEntity, int value);
	void SetTeamTwoAllowedToCapture(int array_index, bool value);
	bool GetTeamTwoAllowedToCapture(int array_index);
	void NormalizeAllowedToCapture(int array_index);
	void SetPointOrigin(edict_t* pEntity, const char* origin_as_string);
	Vector GetPointOrigin(int array_index);

private:
	edict_t* pEntity;
	char point_name[64];
	char point_linkname[64];
	int point_obj_list_index;
	int owned_by_team;
	bool is_team_one_allowed_to_capture;
	bool is_team_two_allowed_to_capture;
	Vector point_origin;
};

extern control_point_t ControlPoints[MAX_CAPTUREPOINTS];


/*
* class for DoD specific capture areas ... often linked to standard control points to allow the multiple teammates needed for capturing feature
*/
class dod_control_point_through_capture_area_t
{
public:
	dod_control_point_through_capture_area_t();
	void ResetArray(void);
	int FindPointInArray(edict_t* pEntity);
	int FindPointByObjListIndex(int searched_point_index);
	int FindPointByName(const char* name);
	void AddNewPoint(edict_t* pEntity);
	void SetPointName(edict_t* pEntity, const char* name);
	const char* GetPointName(int array_index);
	void SetPointObjListIndex(edict_t* pEntity, int value);
	void SetPointObjListIndex(int array_index, int value);
	int GetPointObjListIndex(int array_index);
	void SetDodObjectRequired(edict_t* pEntity, bool value);
	bool GetDodObjectRequired(int array_index);
	void SetOwnedByTeam(edict_t* pEntity, int team_id);
	void SetOwnedByTeam(int array_index, int team_id);
	int GetOwnedByTeam(int array_index);
	void SetTimeToCapture(edict_t* pEntity, float time);
	float GetTimeToCapture(int array_index);
	void SetTeamOnePlayersCurrPresent(int array_index, int number);
	int GetTeamOnePlayersCurrPresent(int array_index);
	void SetTeamTwoPlayersCurrPresent(int array_index, int number);
	int GetTeamTwoPlayersCurrPresent(int array_index);
	void SetTeamOnePlayersToCapture(edict_t* pEntity, int number);
	int GetTeamOnePlayersToCapture(int array_index);
	void SetTeamTwoPlayersToCapture(edict_t* pEntity, int number);
	int GetTeamTwoPlayersToCapture(int array_index);
	void SetTeamOneAllowedToCapture(edict_t* pEntity, int value);
	bool GetTeamOneAllowedToCapture(int array_index);
	void SetTeamTwoAllowedToCapture(edict_t* pEntity, int value);
	bool GetTeamTwoAllowedToCapture(int array_index);

private:
	edict_t* pEntity;
	char point_name[64];
	int point_obj_list_index;
	bool is_dod_object_required;
	int owned_by_team;
	float time_to_capture;
	int team_one_players_currently_present;
	int team_two_players_currently_present;
	int team_one_players_to_capture;
	int team_two_players_to_capture;
	bool is_team_one_allowed_to_capture;
	bool is_team_two_allowed_to_capture;
};

extern dod_control_point_through_capture_area_t dodCaptureArea[MAX_CAPTUREPOINTS];

#endif // BOT_MANAGER_H