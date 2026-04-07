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
// bot.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BOT_H
#define BOT_H

#include "defines.h"

// stuff for Win32 vs. Linux builds

#if defined ( NEWSDKAM ) || defined ( NEWSDKVALVE )

#undef DLLEXPORT
#ifdef _WIN32
#define DLLEXPORT __stdcall
#else
#define DLLEXPORT __attribute__ ((visibility("default")))
#endif

#endif

// the bleending interface definition needs this
#define MAXSTUDIOBONES		128		// total bones actually used


#ifndef __linux__

typedef int (FAR *GETENTITYAPI)(DLL_FUNCTIONS *, int);
typedef int (FAR *GETNEWDLLFUNCTIONS)(NEW_DLL_FUNCTIONS *, int *);
typedef void (DLLEXPORT *GIVEFNPTRSTODLL)(enginefuncs_t *, globalvars_t *);
typedef int (*SERVER_GETBLENDINGINTERFACE) (int, struct sv_blending_interface_s**, struct engine_studio_api_s*, float(*)[3][4], float(*)[MAXSTUDIOBONES][3][4]);
typedef void (FAR *LINK_ENTITY_FUNC)(entvars_t *);

#else

#include <dlfcn.h>
#define GetProcAddress dlsym

typedef int BOOL;

typedef int (*GETENTITYAPI)(DLL_FUNCTIONS *, int);
typedef int (*GETNEWDLLFUNCTIONS)(NEW_DLL_FUNCTIONS *, int *);
typedef void (*GIVEFNPTRSTODLL)(enginefuncs_t *, globalvars_t *);
typedef int (*SERVER_GETBLENDINGINTERFACE) (int, struct sv_blending_interface_s**, struct engine_studio_api_s*, float(*)[3][4], float(*)[MAXSTUDIOBONES][3][4]);
typedef void (*LINK_ENTITY_FUNC)(entvars_t *);

#endif

extern char mod_dir_name[32];
extern int g_mod_version;

// define constants used to identify the MOD version
#define DOD_13			1

extern bool is_steam;

// used to test for existence (e.g. if bot doesn't have any grenade then the slot must use this "no value")
#define NO_VAL			-1

// for development debug file, this file is created in dll.cpp->GameDLLInit()
extern char debug_fname[256];

#define PUBLIC_DEBUG_FILE	"mb_error-log.txt"	// public debug file

// define constant used to limit enemy search range while carrying a goal item
#define ENEMY_DIST_GOALITEM		200.0

// define constant used to check if the enemy is close enough to try a knife attack
#define RANGE_MELEE			300.0

// the maximum of names for each name array that can be used
#define MAX_BOT_NAMES 50

// max name length that can be used for bot (includes chars for '[MB]' tag)
#define BOT_NAME_LEN 31

#define MAX_BOT_WHINE 1				// NOT USED


// total bot difficulty levels (ie max number that can be used for skill arg when spawning bot)
#define BOT_SKILL_LEVELS	5

// used to limit the number of bots that follow one specific team leader (size 4 means that there is 1 team leader and 3 team members)
constexpr int FIRETEAM_SIZE = 4;

// ladder use directions (keep this order otherwise some things may cause wrong behaviour)
enum class LadderClimbDirection
{
	unknown = 0,
	climb_up,
	climb_down
};
typedef LadderClimbDirection LadderDir;

// pBot-> wander_direction constants
#define SIDE_LEFT		1
#define SIDE_RIGHT		2

// bot move speed (keep this order otherwise some things may cause wrong behaviour)
enum class BotMoveSpeed
{
	stop = 0,
	slowest,
	slow,
	max
};
typedef BotMoveSpeed MoveSpeed;

// pBot->respawn_state constants
#define RESPAWN_IDLE             1
#define RESPAWN_NEED_TO_RESPAWN  2
#define RESPAWN_IS_RESPAWNING    3

// VGUI confirmation messages
#define MSG_VGUI_IDLE						1
#define MSG_VGUI_TEAM_SELECT				2
#define MSG_VGUI_CLASS_SELECT_US			3
#define MSG_VGUI_CLASS_SELECT_BRITS			4
#define MSG_VGUI_CLASS_SELECT_AXIS			5
#define MSG_VGUI_CLASS_SELECT_AXIS_PARA		6
#define MSG_VGUI_MOTD_WINDOW				7


// pBot->behaviour constants
// behaviour types
#define STANDARD		(1<<0)		// standard play
#define ATTACKER		(1<<1)		// more aggressive play (short wait times etc.)
#define DEFENDER		(1<<2)		// more defensive play (longer wait times)
// additional behaviour info based on weapon
#define COMMON			(1<<10)		// common rifle
#define CQUARTER		(1<<11)		// bot with SMG & shotgun
#define MGUNNER			(1<<12)		// bot with machinegun
#define SNIPER			(1<<13)		// bot with sniper rifle
#define AASPEC			(1<<14)		// bot with rocket/grenade launcher (anti-armor specialist)
//
#define BOT_PRECISION	(1<<20)		// assigned whenever bot is heading towards waypoint with range below the default range of 50 units
// to handle bot stance/position
#define BOT_DONTGOPRONE	(1<<25)		// temporary prevents going prone in order to allow successful weapon reload, merge magazines etc.
#define GOTO_STANDING	(1<<26)		// should stand up
#define GOTO_CROUCH		(1<<27)		// should go to crouch
#define GOTO_PRONE		(1<<28)		// should go to prone
#define BOT_STANDING	(1<<29)		// bot is in standing position
#define BOT_CROUCHED	(1<<30)		// bot is fully crouched
#define BOT_PRONED		(1<<31)		// bot is in prone

// pBot->bot_tasks constants
// some of these tasks aren't really tasks, they are more of flags or something, but having
// things set this way is probably better than having dozens of unique bool variables
#define TASK_OPPOSITEPATHDIR	(1<<0)	// when bot follows the path in opposite direction (i.e. from path end to path start)
#define TASK_DEATHFALL			(1<<1)	// the bot is tracelining forward direction to detect deep pits
#define TASK_DONTMOVEINCOMBAT	(1<<2)	// the bot is forced to stay on one place (ie. not moving single step forward)
#define TASK_IGNOREWPTNAV		(1<<3)	// the bot will ignore waypoints to finish certain action (eg. a reaction to Hold this position command)
#define TASK_FIRE				(1<<4)	// the bot has to use primary fire out of combat mode
#define TASK_IGNOREAIMWPTS		(1<<5)	// the bot will ignore aim wpts ie. won't target them
#define TASK_PRECISEAIM			(1<<6)	// the bot will try to face the aim wpt really accurately (useful for breakables)
#define TASK_CHECKAMMO			(1<<7)	// the bot has to check his ammo reserves so he knows how many mags should he take from ammobox
#define TASK_GOALITEM			(1<<8)	// the bot is carrying a goal item
#define TASK_SETCLAYMORE		(1<<9)	// bot is going to use the claymore mine
#define TASK_NOJUMP				(1<<10)	// the bot isn't allowed to jump, stamina is low (under 30)
#define TASK_SPRINT				(1<<11)	// the bot has to sprint
#define TASK_WPTACTION			(1<<12)	// the bot is tasked to do some waypoint based action (use something)
#define TASK_BACKTOPATROL		(1<<13)	// the bot has to return back on patrol path (being set when getting to combat while on patrol path)
#define TASK_PARACHUTE			(1<<14)	// the bot has a parachute pack - in DoD it's used to let the bot know he entered Area capture control point typically for more team members
#define TASK_BLEEDING			(1<<15)	// the bot is bleeding
#define TASK_SPEAK				(1<<16)	// the bot has to communicate with others via radio or voice command or hand signal whatever is available
#define TASK_HEALHIM			(1<<17)	// the bot is going to heal someone (someone called for medic)
#define TASK_MEDEVAC			(1<<18)	// the bot has to treat downed teammate - in DoD this is used to make bot wait for a while after dropping the box with extra ammo for a teammate
#define TASK_FIND_ENEMY			(1<<19)	// the bot has to find different enemy then the one he has right now
#define TASK_USETANK			(1<<20)	// the bot is using a mounted gun (ie TANK)
#define TASK_BIPOD				(1<<21)	// the bot just used the bipod on current weapon (ie. can't move, limited pitch&yaw)
#define TASK_CLAY_IGNORE		(1<<22)	// the bot has to ignore this claymore - in DoD this is used to make the bot get to exact location of the explosives charge upon reaching the ammobox waypoint
#define TASK_CLAY_EVADE			(1<<23)	// the bot has to evade this claymore - in DoD it's used to make the bot know he will have to run away from planted explosives charge
#define TASK_GOPRONE			(1<<24)	// the bot has to go prone
#define TASK_AVOID_ENEMY		(1<<25)	// the bot has to ignore distant enemies
#define TASK_IGNORE_ENEMY		(1<<26)	// the bot has to ignore all enemies except those who are right next to him
#define TASK_USE				(1<<27)

// pBot->bot_subtasks constants
#define ST_AIM_GETAIMWPT	(1<<0)	// got to select one aim waypoint from the array of nearby aim waypoints as current aim target
#define ST_AIM_FACEAIMWPT	(1<<1)	// to face his current aim waypoint/target
#define ST_AIM_SETTIME		(1<<2)	// to decide the time that will be used to keep aiming at current aim waypoint/target
#define ST_AIM_ADJUSTAIM	(1<<3)	// to check if he's really facing current aim
#define ST_AIM_DONE			(1<<4)	// is facing current aim waypoint
#define ST_FACEGENT_DONE	(1<<5)	// is facing the entity stored as a PointerToGENT (pointer to some other game entity being it some item, button etc.)
#define ST_FACEPOINTIS_DONE	(1<<6)	// is facing the coordinates stored in Point in Space
#define ST_BUTTON_USED		(1<<7)	// the bot just successfully used some usable object
#define ST_PARACHUTE_USED	(1<<8)	// the bot has opened the parachute - in DoD it's used to randomly wait at a pushpoint waypoint that's placed in Area capture control point
#define ST_MEDEVAC_ST		(1<<9)	// trying to medevac corpse (aiming to its stomach) - in DoD it's used to tell bots with rocket/grenade launcher to switch to it to break the entity
#define ST_MEDEVAC_H		(1<<10)	// trying to medevac corpse (aiming to its head) - in DoD it's used to mark the case when bot reacts to a "Use the bazooka" command
#define ST_MEDEVAC_F		(1<<11)	// trying to medevac corpse (aiming to its feet) - in DoD it's used to mark the case when bot reacts to a "Use the bazooka" command where teammate points to breakable target
#define ST_MEDEVAC_DONE		(1<<12)	// the bot successfully finished the medevac
#define ST_TREAT			(1<<13)	// the bot is trying to stop patient's bleeding
#define ST_HEAL				(1<<14)	// the bot is trying to restore patient's health
#define ST_GIVE				(1<<15)	// the bot is trying to give bandages to his patient
#define ST_HEALED			(1<<16)	// the bot finished the medical treatment
#define ST_SAY_CEASEFIRE	(1<<17)	// the bot has to say a cease fire team message
#define ST_FACEENEMY		(1<<18) // the bot is currently facing his enemy (also possible new enemy)
#define ST_DOOR_OPEN		(1<<19) // the bot is passing doors
#define ST_TANK_SHORT		(1<<20) // the bot is forced to use the mounted gun only for given time period (ie. waypoint wait time is set)
#define ST_RANDOMCENTRE		(1<<21)	// we need to randomize the final point the bot is aiming at (ie. a small range around the original point), useful if the bot has to target a breakable object that has a hole in its middle
#define ST_USEEYESORIGIN	(1<<22)	// the bot will use origin+head (ie. look through eyes) to make the vector to target entity instead of only origin, especially useful for low breakables like on sd_durandal
#define ST_NOOTHERNADE		(1<<23)	// checked that the bot doesn't carry more than one grenade type
#define ST_W_CLIP			(1<<24)	// the bot has to check how much ammo is left in the clip
#define ST_CANTPRONE		(1<<25)	// bot cannot go prone ... engine prevents it due to being too close to a wall for example
#define ST_EVASIONSTARTED	(1<<26)	// bot started one evasion action (eg. will jump over claymore)
//
#define ST_INAREA			(1<<30)	// the bot is within one of DoD specific client areas (ie. either 'place charge here' or 'capture flag with a teammate')
#define ST_GOALITEM_BOMB	(1<<31)	// the goal item bot carries is a bomb (ie. satchel/tnt charge)

//pBot->bot_needs constants
#define NEED_POSTSPAWN_DECISIONS	(1<<0)	// need to make post spawn decisions (ie. whether he needs a goal or look for bandages etc.)
#define NEED_GOAL					(1<<1)	// the bot is in mood for reaching the map objective
#define NEED_AMMO					(1<<2)	// the bot has low ammo and needs to take some more
#define NEED_NEXTWPT				(1<<3)	// the bot needs to get next waypoint right at the moment he can call navigation again (without checking whether he really reached current one or not)
#define NEED_RESETNAVIG				(1<<4)	// the bot needs to reset current waypoint and path
#define NEED_RESETCLAYMORE			(1<<5)	// the bot needs to reset claymore usage, because he wasn't able to place it correctly (ie. was too close to some object)
#define NEED_BANDAGES				(1<<6)	// the bot has no (common soldiers) or low amount (medics) of bandages and needs some more
#define NEED_BANDAGES_NOT			(1<<7)	// common bot decided not to look for bandages at all
#define NEED_COMMITSUICIDE			(1<<8)	// bot is completely stuck, tried changing path direction to get free, but if that failed too then he needs to kill self
#define NEED_AIR					(1<<9)	// the bot needs air because he's currently under water and is drowning
#define NEED_FIRETEAM				(1<<10)	// the bot with leadership skill needs to form a fireteam
#define NEED_FIRETEAM_NOT			(1<<11)	// the bot decided to ignore fireteams

#define NEED_EXLOSIVESCHARGE		(1<<30)	// DoD specific need that replaces the need for ammo when it comes to ammobox wpt and ammo path tag in the navigation system
//#define 
#define NEED_RESETPARACHUTE			(1<<31) // the bot needs to check if parachute is finally gone (for FA 2.65 and below)

// pBot->bot_flags contants
#define BF_NAMECHECK_DONE			(1<<0)	// when the test for bot name matching the nation in allied team (british vs american) is done

#define BF_NOT_JOINED_GAME			(1<<10)	// forces bot to select team and class (gear configuration) before joining the game
#define BF_MUST_BE_INITIALIZED		(1<<11)	// ensures bot variables will be reset to default values when bot dies or map goal is reached and the round restarts
#define BF_RESPAWN_AT_ROUND_END		(1<<12)	// allows bot to respawn correctly when a map goal is reached (Firearms doesn't seem to have specific round end event)
#define BF_RESPAWN_TRY_IT_AGAIN		(1<<13)	// bot needs to go through "join the game" again, because he wasn't able to spawn correctly (eg. uneven teams problem)

// constants used to detect the status of edict->v.fov (to detect scoped weapons zoom level)
#define ZOOM_NO		0.0		// standard view
#define ZOOM_1X		20.0
#define ZOOM_NOT	90.0	// standard view

// constants used to detect the status of edict->v.flags (those not in const.h, but specific to Firearms)
#define FL_BROKENLEG	(1<<28)		// player has broken his leg (slower movement)

// constants used to detect the status of edict->v.iuser3
#define USR3_PRONE				1		// player lays prone
#define USR3_BIPOD_USED			2		// player lays prone and has the weapon on bipod

// edict->v.vuser1.x constants
#define VUSR1_BIPOD_SPOT			1.0		// player is in a spot where he can bipod his weapon
#define VUSR1_BIPOD_USED			2.0		// player is inside bipod suitable spot and already has the weapon on bipod

// pBot->weapon_action constants
#define W_LOCKED			0		// weapon cannot be used at all - to prevent bot do any weapon actions right after spawning into the game (especially FA 2.4 & 2.5 need this)
#define W_READY				1		// ready to shoot
#define W_TAKEOTHER			2		// change to another weapon
#define W_INCHANGE			3		// in process of taking other weapon
#define W_INHANDS			4		// the weapon change is almost finished (current weapon message sent appropriate ID)
#define W_INRELOAD			5		// reloading it
#define W_INMERGEMAGS		6		// merging magazines
//#define W_INMOUNT					// mounting silencer	// not used

// pBot->weapon_status constants
//#define WS_			(1<<0)		// UNUSED ATM
#define WS_CHECKWEAPON			(1<<1)		// bot checks his current weapon for silencer and fire mode (useful after weapon switch)
#define WS_SILENCERCHECKED		(1<<2)		// bot did check for silencer support on his current weapon
//#define WS_		(1<<3)		// UNUSED ATM
#define WS_MOUNTSILENCER		(1<<4)		// bot will mount the silencer once the weapon is ready to be handled
#define WS_PRESSRELOAD			(1<<5)		// to know that bot just "pressed reload button" (set IN_RELOAD to Edict->v.button)
#define WS_INVALID				(1<<6)		// weapon based action (e.g. reloading) was invalidated due to something (e.g. someone started to heal this bot)
#define WS_NOTEMPTYMAG			(1<<7)		// when the bot is going to reload weapon with not completely empty magazine (used to detect the need to merge magazines)
#define WS_MERGEMAGS1			(1<<8)		// first almost empty magazine was reloaded
#define WS_MERGEMAGS2			(1<<9)		// second almost empty magazine was reloaded (now the bot needs to merge magazines)
#define WS_NOAMMOFORMAIN		(1<<10)		// there are no magazines to reload main weapon
#define WS_NOAMMOFORBACKUP		(1<<11)		// there are no magazines to reload backup weapon
#define WS_SECONDARYMODEACTIVE	(1<<12)		// when current weapon is switched to secondary fire mode (e.g. using attached grenade launcher or using the optics)
#define WS_RELOADSECONDARY		(1<<13)		// in DoD it's used to postpone bipod deployment after shooting the machine gun in out of combat mode otherwise it would not be registered
#define WS_CANTBIPOD			(1<<14)		// when bipod cannot be deployed (e.g. bot is standing in open area)
#define WS_CLAYMOREGOAL			(1<<15)		// claymore mine will be manually detonated to destroy map goal object
#define WS_CLAYMORETRIP			(1<<16)		// claymore mine will be used in trip mode
#define WS_CLAYMOREDONE			(1<<17)		// claymore mine is placed and activated or detonated
#define WS_GRENADEAVAILABLE		(1<<18)		// bot has at least one grenade available
#define WS_GRENADEDEPLETED		(1<<19)		// bot already used all his grenades
#define WS_GRENADEPINPULLED		(1<<20)		// to handle the safety pin removal action in Firearms and above
//#define WS_GRENADEPRIMING		(1<<21)		// UNDONE - alternate way to throw the grenade (releases the lever and lets the fuze burn partially)
#define WS_DONTSWITCHTOOTHER	(1<<22)		// used to prevent deselecting temporarily used weapon to destroy the breakable object when bot is waiting at a shoot waypoint (eg. launcher on class based path)
#define WS_BIPODMANIPULATION	(1<<23)		// used to check whether the bipod deploying or folding action was successfully finished
#define WS_DROPAMMO				(1<<24)		// assigned when the bot drops the box with extra ammo for a teammate

#ifdef DEBUG
#define WS_TEST_DONTCHECKAMMO	(1<<29) // just for debugging purposes, feel free to erase it if needed be
#define WS_TEST_INATTACK		(1<<30) // just for debugging purposes, feel free to erase it if needed be
#define WS_TEST_INATTACK2		(1<<31) // just for debugging purposes, feel free to erase it if needed be
#endif // DEBUG

// DoD specific spawn flags for breakable entities
#define SF_BREAK_ALLIES_ONLY		(1<<4)	// breakable only by allied team
#define SF_BREAK_AXIS_ONLY			(1<<5)	// breakable only by axis team
#define SF_BREAK_OBJECT_CAP_ONLY	(1<<6)	// breakable by dod object only (eg. tnt charge on the bridge on map Escape)
#define SF_BREAK_ROCKET_ONLY		(1<<9)	// breakable only by rocket launcher (eg. bazooka)


// for pBot->used_weapon
enum class BotUseWeapon
{
	none = 0,
	main,
	backup,
	knife,
	grenade,
	claymoremine
};
typedef BotUseWeapon uWeapon;

// for pBot->DecideNextWeapon function
enum class DecideWeaponCheckFoeDistance
{
	dontcheck = 0,
	checkit
};
typedef DecideWeaponCheckFoeDistance foeDist;

// used to allow bots use voice commands and/or radio and/or hand signals (where/if available)
enum class BotVoiceCommand
{
	nothing = 0,
	area_clear,
	get_down,
	enemy_ahead,
	coverme,			// in DoD this is "I need backup"
	yes_sir,
	negative,
	medic,
	fire_in_the_hole,
	grenade,
	handsig_yes_sir,
	handsig_negative
};
typedef BotVoiceCommand voiceCmd;

// used for the text messages bot needs to say via say or say_team commands
enum class BotTextMessage
{
	nothing = 0,
	grenade_in,
	grenade_out,
	claymore_found,
	medic_help_you,
	medic_cant_help,
	cease_fire,
	enemy_spotted
};
typedef BotTextMessage botSay;

// visibility return constants
#define VIS_NO			0		// not visible (like bool FALSE)
#define VIS_YES			1		// visible (like bool TRUE)
#define VIS_FENCE		2		// visible but behind fence like object so only big caliber gun can shoot through
#define VIS_WATER		3		// visible but looking through water so a check for the transparency is needed (TODO: implement it - now it just makes the bot ignore/forget such enemy)

// used to detect if bot is close enough to any entity bot searched
#define STANDARD_SEARCH_RADIUS		70.0f
#define EXTENDED_SEARCH_RADIUS		400.0f
#define TEAMMATE_SEARCH_RADIUS		300.0f
#define FIND_ITEM_RADIUS			200.0f


constexpr int teamNULL = -1;		// neither team assigned, also has to be different from 0 (zero) to prevent possible errors, because 0 is used in the mod
constexpr int modteamsnameSize = 16;

// class of functions to define the team names used for various cases
class modTeams
{
public:
	inline void SetTeamId(int newValue) { teamId = newValue; sprintf(teamIdAsStr, "%d", newValue); }
	inline int GetTeamId(void) { return teamId; }
	inline char* GetTeamIdAsString(void) { return teamIdAsStr; }
	inline void SetTeamName(const char* newName) { strcpy(teamName, newName); }	// plain one word name
	inline char* GetTeamName(void) { return teamName; }
	inline void SetTeamName2wordsLC(const char* newName) { strcpy(teamName2wordslc, newName); }	// two words lowercase version
	inline char* GetTeamName2wordsLC(void) { return teamName2wordslc; }
	inline void SetTeamName2wordsFUC(const char* newName) { strcpy(teamName2wordsfuc, newName); }	// version with two words and 1st letters uppercase
	inline char* GetTeamName2wordsFUC(void) { return teamName2wordsfuc; }
	inline void SetTeamNameAltF1word(const char* newName) { strcpy(teamNameAltf1word, newName); }	// one word version with alternative formatting
	inline char* GetTeamNameAltF1word(void) { return teamNameAltf1word; }
	inline void SetTeamNameForGoalAltF1word(const char* newName) { strcpy(teamNameGoalAltf, newName); }	// one word version with alt. formatting for team goal
	inline char* GetTeamNameForGoalAltF1word(void) { return teamNameGoalAltf; }
	
	inline void SetTeamNameForPlayerModel(const char* newName) { strcpy(teamNamePlayerModel, newName); } // string used for player model for this team
	inline char* GetTeamNameForPlayerModel(void) { return teamNamePlayerModel; }
	inline void SetTeamNameNetname(const char* newName) { strcpy(teamNameNetname, newName); }	// string used for netname for this team
	inline char* GetTeamNameNetname(void) { return teamNameNetname; }

	inline void SetTeamPathColor(vec_t red, vec_t green, vec_t blue) { teamPathColor.x = red; teamPathColor.y = green; teamPathColor.z = blue; }
	inline Vector GetTeamPathColor(void) { return teamPathColor; }

private:
	int teamId = teamNULL;
	char teamIdAsStr[4] = "n/a";
	char teamName[modteamsnameSize] = "no_name";
	char teamName2wordslc[modteamsnameSize] = "no_name";
	char teamName2wordsfuc[modteamsnameSize] = "no_name";
	char teamNameAltf1word[modteamsnameSize] = "no_name";	
	char teamNameGoalAltf[modteamsnameSize] = "no_name";
	char teamNamePlayerModel[modteamsnameSize] = "no_name";
	char teamNameNetname[modteamsnameSize] = "no_name";
	Vector teamPathColor = g_vecZero;
};

typedef modTeams modTeams_t;

extern modTeams_t teamONE;	// red team in Firearms
extern modTeams_t teamTWO;	// blue team in Firearms

typedef struct
{
   int	isActive;	// 1 if this weapon is in hands
   int  iId;		// weapon ID
   int  iClip;		// amount of ammo in the clip
   int	iAttachment;// supressor (0-normal, 1-silenced <- bugged in FA) or gl (ammo2 in chamber)		Not available in DoD
   int	iFireMode;	// fire mode (1 - semi || 2 - 3rburst || 4 - auto || 0 - where no choice)			Not available in DoD
   int  iAmmo1;		// amount of ammo in primary reserve (i.e. mags)
   int  iAmmo2;		// amount of ammo in secondary reserve (i.e. mags)									Not available in DoD
} bot_current_weapon_t;

class HistoryPoint
{
public:
	HistoryPoint() { index = -1; next = prev = NULL; }
	int index;
	HistoryPoint *next;
	HistoryPoint *prev;
};

/*
  kota@
  circle wpt history.
*/
class WptHistory 
{
private:
	HistoryPoint *start, *end;
	int size;
public:
	WptHistory()
	{
		size=5;
		HistoryPoint *cur;
		start = end = new HistoryPoint;
		
		for (int i=0; i<size; ++i) 
		{
			cur = new HistoryPoint;
			cur->next = end;
			end->prev = cur;
			end = cur;
		}
		end->prev = start;
		start->next = end;
		start=end;
	}
	~WptHistory()
	{
		//FILE *f = fopen("\\debug1.txt", "a+");
		//fprintf(f, "==@= desctructor\n");
		//fclose(f);
		HistoryPoint *cur, *tmp;
		cur = start;
		while (cur->next == start){
			tmp = cur;
			cur = cur->next;
			delete tmp;
		}
		delete cur; //the last element.
		end = start = NULL;
	}
	void print(){
#ifdef _DEBUG
		HistoryPoint *cur;
		//FILE *f = fopen("\\debug1.txt", "a+");
		//fprintf(f, "==*= %p print list\n", this);
		//fclose(f);
		cur = start;
		do {
			//f = fopen("\\debug1.txt", "a+");
			//fprintf(f, "cur->prev=%p, cur =%p, cur->next=%p\n", cur->prev, cur, cur->next);
			//fclose(f);
			cur = cur->next;
		} while (cur!=start);
		//f = fopen("\\debug1.txt", "a+");
		//fprintf(f, "===========\n");
		//fclose(f);
#endif	
	}
	inline int get(int j=0) {
		HistoryPoint *cur = start;
		for (int i=0; i<j && cur!=end; ++i)
		{
			cur = cur->next;
		}
		return (cur!=end) ? cur->index : -1;
	}
	inline void clear() {
		end = start;
		end->index = -1;
	}
	inline void push(int wpt_index) {
		start = start->prev;
		start->index = wpt_index;
		if (start==end)
		{
			end = end->prev;
		}
		
	}
	bool check(int wpt_index)
	{
		HistoryPoint *cur = start;
		while (cur != end) 
		{
			if (cur->index == wpt_index)
			{
				return true;
			}
			cur = cur->next;
		}
		return false;
	}
};


class AimWptIndex_t
{
public:
	AimWptIndex_t();
	void AddNewAimWpt(int newVal);
	void Clear(void);
	int Count(void);
	int Get(int array_index = 0);
	inline int GetRandom(void) { return Get( RANDOM_LONG(1, Count()) - 1 ); };		// -1 to make it a valid array index, because Count() returns the number of used array slots
	int Print(int array_index = 0);													// by default print the first array slot
	inline bool IsEmpty(void) { return (aim_index[0] == NO_VAL); }			// the array is being filled from first to last so we only need to check this slot to know it for sure

private:
	static const int size = 4;				// the number of different aiming waypoints the bot can target (not at one moment of course)
	int aim_index[size];
};


class bot_t
{
public:
	bot_t();
	void BotSpawnInit(void);
	void InitializeAtCreation(void);
	float CalculateMovedDistanceSinceLastCheck(void);
	float ConvertMoveSpeedToRealValue(void);
	void BotThink(void);
	void BotStartGame(void);
	void ResetPosture(void);
	void ResetStance(void);
	bool GoProne(const char* loc);
	void SetStance(int flag, const char* loc = NULL);
	void SetStance(int flag, bool is_forced_stance, const char* loc = NULL);
	void BotSpeak(voiceCmd use_this_command, float set_delay = 0.0f);
	void UseTextMessage(botSay text_message, edict_t* pRecipient = NULL, float since_last_msg = 1.0f);
	bool UpdateSounds(edict_t *pPlayer);
	bool CanResetPitch(void);
	bool FaceGameEntity(void);
	float GetDistanceToGEnt(void);
	void FacePointInSpace(void);
	void ResetAims(const char* loc = NULL);
	void TargetAimWaypoint(const char* loc = NULL);
	void BotWaitHere(void);
	float GenerateHoldPositionTime(void);
	bool CheckMainWeaponOutOfAmmo(const char* loc = NULL);
	bool CheckBackupWeaponOutOfAmmo(const char* loc = NULL);
	void SetWeaponIsOutOfAmmo(const char* loc = NULL);
	bool IsCurrentWeaponEmpty(void);
	bool ShouldReload(const char* loc);
	void ReloadWeapon(const char* loc = NULL);
	void CheckAmmoReserves(const char* loc);
	bool DecideNextWeapon(const char* loc = NULL, foeDist need_dist_check = foeDist::dontcheck, float foe_distance = 9999.0f);
	void ChangeWeapon(void);
	void UseMainWeapon(const char* loc = NULL);
	void UseBackupWeapon(const char* loc = NULL);
	void UseKnife(const char* loc = NULL);
	bool UseGrenade(const char* loc = NULL);
	float GetWeaponEffectiveRange(int weapon_index = NO_VAL);
	float GetWeaponSafeRange(int weapon_index = NO_VAL);
	bool CanUseGrenade(float enemy_distance);
	void BotForgetEnemy(void);
	edict_t* BotFindEnemy(void);
	
	inline void SetBotTeam(int newTeam)					{ in_team = newTeam; }
	inline int GetBotTeam(void)							{ return in_team; }
	inline bool IsBotTeam(int team)						{ return (in_team == team); }
	inline void SetBotClass(int newClass)				{ bot_class = newClass; }
	inline int GetBotClass(void)						{ return bot_class; }
	inline void SetFaceSkin(int newSkin)				{ face_skin = newSkin; }
	inline int GetFaceSkin(void)						{ return face_skin; }
	inline void SetBotSkill(int newVal)					{ bot_skill = newVal; }
	inline int GetBotSkill(void)						{ return bot_skill; }
	inline void SetAimSkill(int newVal)					{ aiming_skill = newVal; }
	inline int GetAimSkill(void)						{ return aiming_skill; }
	inline void SetBehaviour(int behaviour)				{ bot_behaviour |= behaviour; }
	void RemoveBehaviour(int behaviour)
	{
		if (bot_behaviour & behaviour)
			bot_behaviour &= ~behaviour;
	}
	inline bool IsBehaviour(int behaviour)				{ return (bot_behaviour & behaviour) == behaviour; }
	inline void SetTask(int task)						{ bot_tasks |= task; }
	void RemoveTask(int task)
	{
		if (bot_tasks & task)
			bot_tasks &= ~task;
	}
	inline bool IsTask(int task)						{ return (bot_tasks & task) == task; }
	inline void SetSubTask(int subtask)					{ bot_subtasks |= subtask; }
	void RemoveSubTask(int subtask)
	{
		if (bot_subtasks & subtask)
			bot_subtasks &= ~subtask;
	}
	inline bool IsSubTask(int subtask)					{ return (bot_subtasks & subtask) == subtask; }
	inline void SetNeed(int need)						{ bot_needs |= need; }
	void RemoveNeed(int need)
	{
		if (bot_needs & need)
			bot_needs &= ~need;
	}
	inline bool IsNeed(int need)						{ return (bot_needs & need) == need; }
	inline void SetFASkill(int newFASkill)				{ bot_fa_skills |= newFASkill; }
	inline bool IsFASkill(int faskill)					{ return (bot_fa_skills & faskill) == faskill; }
	inline void SetBotFlag(int botflag)					{ bot_flags |= botflag; }
	void RemoveBotFlag(int botflag)
	{
		if (bot_flags & botflag)
			bot_flags &= ~botflag;
	}
	inline bool IsBotFlag(int botflag)					{ return (bot_flags & botflag) == botflag; }

	inline void SetBotSpawnTime(void)					{ bot_spawn_time = gpGlobals->time; }
	inline bool IsTimeSinceBotSpawned(float time_in_seconds) { return (bot_spawn_time + time_in_seconds < gpGlobals->time); }
	
	inline void SetHealth(int newVal)					{ current_health = newVal; }
	inline int GetHealth(void)							{ return current_health; }
	inline void UpdatePrevHealth(void)					{ previous_health = current_health; }
	inline int GetPrevHealth(void)						{ return previous_health; }
	inline bool IsLosingHealth(void)					{ return (current_health < previous_health); }
	inline void SetAmountOfBandages(int newVal)			{ amount_of_bandages = newVal; }
	inline int GetAmountOfBandages(void)				{ return amount_of_bandages; }
	inline void SetBandageTime(float time_in_seconds)	{ bandage_time = gpGlobals->time + time_in_seconds; }
	inline float GetBandageTime(void)					{ return bandage_time; }
	inline bool IsBandagingNow(void)					{ return (bandage_time >= gpGlobals->time); }
	inline void DontAllowBandaging(void)				{ bandage_time = -1.0f; }
	inline bool IsBandagingAllowed(void)				{ return (bandage_time != -1.0f); }
	inline void SetMedicTreatTime(float time_in_seconds) { medic_treat_time = gpGlobals->time + time_in_seconds; }
	inline float GetMedicTreatTime(void)				{ return medic_treat_time; }
	inline bool IsMedicalTreatmentNow(void)				{ return (medic_treat_time >= gpGlobals->time); }

	inline void SetPausedTime(float time_in_seconds)	{ paused_time = gpGlobals->time + time_in_seconds; }
	inline float GetPausedTime(void)					{ return paused_time; }
	inline bool IsNotPaused(float added_delay = 0.0f)	{ return ((paused_time + added_delay) < gpGlobals->time); }
	inline void SetGroundItemPosition(Vector item_position) { ground_item_pos = item_position; }
	inline Vector GetGroundItemPosition(void)			{ return ground_item_pos; }

	inline void SetMaxSpeed(float newVal)				{ f_max_speed = newVal; }
	inline float GetMaxSpeed(void)						{ return f_max_speed; }
	inline void UpdatePrevMoveSpeed(void)				{ prev_movespeed = current_movespeed; }
	inline MoveSpeed GetPrevMoveSpeed(void)				{ return prev_movespeed; }
	inline void SetMoveSpeed(MoveSpeed newVal)			{ current_movespeed = newVal; }
	inline MoveSpeed GetMoveSpeed(void)					{ return current_movespeed; }
	inline void SetDontMoveTime(float time_in_seconds)	{ dont_move_time = gpGlobals->time + time_in_seconds; }
	//inline void ClearDontMoveTime(void)					{ dont_move_time = gpGlobals->time; }								NOT USED
	inline bool IsDontMoveTime(void)					{ return (dont_move_time >= gpGlobals->time); }
	inline void StrafeLeftFor(float time_in_seconds)	{ strafe_time = gpGlobals->time + time_in_seconds; strafe_direction = -1.0f; }
	inline void StrafeRightFor(float time_in_seconds)	{ strafe_time = gpGlobals->time + time_in_seconds; strafe_direction = 1.0f; }
	inline bool DoesBotStrafeNow(void)					{ return (strafe_time >= gpGlobals->time); }
	void SetDontCheckStuck(const char* loc = NULL, float time_in_seconds = 1.0f);
	inline bool IsTimeToCheckStuck(void)				{ return (dont_check_stuck_time < gpGlobals->time); }
	inline void SetGotStuckTime(void)					{ got_stuck_time = gpGlobals->time; }
	inline float GetGotStuckTime(void)					{ return got_stuck_time; }
	inline bool NotBeenStuckFor(float time_in_seconds)	{ return ((got_stuck_time + time_in_seconds) < gpGlobals->time); }
	inline void IncUnstuckAttempts(int increment = 1) { unstuck_attempts = unstuck_attempts + increment; IncWaypointPenalty(increment * 20.f); }
	inline void ResetUnstuckAttempts(void)				{ unstuck_attempts = 0; }
	inline int GetUnstuckAttempts(void)					{ return unstuck_attempts; }
	
	inline void SetDontLookForWaypoint(float time_in_seconds = 0.0f) { dont_look_for_waypoint_time = gpGlobals->time + time_in_seconds; }
	inline bool CanLookForWaypoint(void)				{ return (dont_look_for_waypoint_time < gpGlobals->time); }
	void ResetWaypointBasedNavigation(void);
	void SetCurrentWaypoint(int wpt_index);
	void ClearCurrentWaypoint(void);
	inline int GetCurrentWaypoint(void)					{ return curr_wpt_index; }
	inline void SetCurrWptPosition(Vector pos)			{ curr_wpt_fake_position = pos; }
	inline Vector GetCurrWptPosition(void)				{ return curr_wpt_fake_position; }
	inline Vector* GetPointerToCurrWptPosition(void)	{ return &curr_wpt_fake_position; }
	inline void SetPrevDistToCurrentWaypoint(float distance) { prev_distance_to_curr_wpt = distance; }
	inline float GetPrevDistToCurrentWaypoint(void)		{ return prev_distance_to_curr_wpt; }
	inline void SetTimeToReachCurrWaypoint(void)		{ time_to_reach_curr_wpt = gpGlobals->time; }
	inline float GetTimeToReachCurrWaypoint(void)		{ return time_to_reach_curr_wpt; }
	inline bool NotReachedCurrWaypointFor(float time_in_seconds) { return ((time_to_reach_curr_wpt + time_in_seconds) < gpGlobals->time); }
	inline void SetFaceWaypointTime(float time_in_seconds) { time_to_face_waypoint = gpGlobals->time + time_in_seconds; }
	inline bool IsNotTurningToFaceWaypoint(void)		{ return (time_to_face_waypoint < gpGlobals->time); }
	inline void SetActionTime(float time_in_seconds)	{ wpt_action_time = gpGlobals->time + time_in_seconds; }
	inline float GetActionTime(void)					{ return wpt_action_time; }
	inline bool IsActionTime(void)						{ return (wpt_action_time >= gpGlobals->time); }
	inline bool NotDoneActionFor(float time_in_seconds) { return ((wpt_action_time + time_in_seconds) < gpGlobals->time); }
	inline void SetWaitTime(float time_in_seconds)		{ bot_wait_time = gpGlobals->time + time_in_seconds; }
	inline float GetWaitTime(void)						{ return bot_wait_time; }
	inline bool IsWaitTime(void)						{ return (bot_wait_time >= gpGlobals->time); }
	inline bool NotBeenWaitingFor(float time_in_seconds) { return ((bot_wait_time + time_in_seconds) < gpGlobals->time); }
	inline void SetPointerToGEnt(edict_t* pentity)		{ pGameEntity = pentity; }
	inline edict_t* GetPointerToGEnt(void)				{ return pGameEntity; }
	inline bool HasNoGEnt(void)							{ return (pGameEntity == NULL); }
	inline bool IsPointerToGEntThisEntity(const char* ent_classname)	{ return ((pGameEntity != NULL) && (strcmp(ent_classname, STRING(pGameEntity->v.classname)) == 0)); }
	inline void SetTimeToFaceGEnt(float time_in_seconds) { time_to_face_game_entity = gpGlobals->time + time_in_seconds; }
	inline float GetTimeToFaceGEnt(void)				{ return time_to_face_game_entity; }
	inline bool IsTurningToFaceGEnt(void)				{ return (time_to_face_game_entity >= gpGlobals->time); }
	inline void SetPositionOfPointInSpace(Vector position_on_map)	{ point_in_space = position_on_map; }
	inline Vector GetPositionOfPointInSpace(void)		{ return point_in_space; }
	inline Vector* GetPointerToPositionOfPointInSpace(void)		{ return &point_in_space; }
	inline void SetCurrentAimWaypoint(int wpt_index)	{ curr_aim_index = wpt_index; }
	inline int GetCurrentAimWaypoint(void)				{ return curr_aim_index; }
	inline void SetTimeToKeepCurrentAim(float time_in_seconds) { time_to_keep_current_aim = gpGlobals->time + time_in_seconds; }
	inline float GetTimeToKeepCurrentAim(void)			{ return time_to_keep_current_aim; }
	inline bool IsTimeToKeepCurrentAim(void)			{ return (time_to_keep_current_aim >= gpGlobals->time); }
	inline void SetTimeOfAdjustingAim(float time_in_seconds = 0.0f) { time_of_adjusting_the_aim = gpGlobals->time + time_in_seconds; }
	inline float GetTimeOfAdjustingAim(void)			{ return time_of_adjusting_the_aim; }
	inline bool IsTimeToAdjustTheAim(void)				{ return (time_of_adjusting_the_aim < gpGlobals->time); }
	inline void SetDuckJumpTime(float time_in_seconds)	{ duckjump_time = gpGlobals->time + time_in_seconds; }
	inline bool IsDoingDuckJumpNow(void)				{ return (duckjump_time >= gpGlobals->time); }
	inline void SetLadderUseDirection(LadderDir newVal) { ladder_use_direction = newVal; }
	inline LadderDir GetLadderUseDirection(void)		{ return ladder_use_direction; }
	inline bool LadderDirectionNotDecidedYet(void)		{ return (ladder_use_direction == LadderDir::unknown); }
	inline bool IsClimbLadderUp(void)					{return (ladder_use_direction == LadderDir::climb_up); }
	inline bool IsClimbLadderDown(void)					{ return (ladder_use_direction == LadderDir::climb_down); }
	inline void SetStartOfUsingLadder(void)				{ start_ladder_time = gpGlobals->time; }
	inline bool IsTimeSinceStartOfUsingLadder(float time_in_seconds) { return ((start_ladder_time + time_in_seconds) < gpGlobals->time); }
	inline void SetReachedEndOfLadder(void)				{ reached_end_of_ladder = true; }
	inline void ResetReachedEndOfLadder(void)			{ reached_end_of_ladder = false; }
	inline bool HasReachedEndOfLadder(void)				{ return reached_end_of_ladder; }
	inline void SetParachuteUseTime(float time_in_seconds) { parachute_use_time = gpGlobals->time + time_in_seconds; }
	inline float GetParachuteUseTime(void)				{ return parachute_use_time; }
	inline void ResetParachuteUseTime(void)				{ parachute_use_time = 0.0f; }
	inline bool HasPassedThroughParachuteWaypoint(void)	{ return (parachute_use_time != 0.0f); }
	inline bool IsParachuteUseTimeOver(void)			{ return (parachute_use_time < gpGlobals->time); }
	inline void SetGoProneTime(void)					{ go_prone_time = gpGlobals->time; }
	inline float GetGoProneTime(void)					{ return go_prone_time; }
	inline bool IsNotGoingProne(void)
	{
		// time from calling the command to a moment when DoD allows firing the gun (based on some game test)
		return (go_prone_time + 1.75f < gpGlobals->time);
	}
	inline void ResetGoProneTime(void)					{ go_prone_time = 0.0f; }
	inline bool IsProne(void)							{ return ((pEdict->v.iuser3 == USR3_PRONE) || (pEdict->v.iuser3 == USR3_BIPOD_USED)); }
	inline bool IsCrouched(void)							{ return ((pEdict->v.flags & FL_DUCKING) && (pEdict->v.iuser3 != USR3_PRONE) && (pEdict->v.iuser3 != USR3_BIPOD_USED)); }

	inline void SetPatrolPathWaypoint(int wpt_index)	{ patrol_path_waypoint = wpt_index; }
	inline int GetPatrolPathWaypoint(void)				{ return patrol_path_waypoint; }
	inline bool IsPatrolPathWaypoint(void)				{ return (patrol_path_waypoint != NO_VAL); }

	int GetNextWaypointOnPath(void);
	bool ReturnBackToPathStart(const char* loc = NULL);
	void MakeRandomTurn(void);
	bool IsInCrampedSpace(void);

	inline void SetSeeTeamLeaderTime(void)				{ bot_see_team_leader_time = gpGlobals->time; }
	inline bool HasSeenTeamLeaderInLast(float time_in_seconds) { return ((bot_see_team_leader_time + time_in_seconds) > gpGlobals->time); }

	inline void SetSeeEnemyTime(void)					{ bot_see_enemy_time = gpGlobals->time; }
	inline bool HasSeenEnemyInLast(float time_in_seconds) { return ((bot_see_enemy_time > 0.0f) && ((bot_see_enemy_time + time_in_seconds) > gpGlobals->time)); }
	inline bool NotSeenEnemyfor(float time_in_seconds = 0.0f) { return ((bot_see_enemy_time > 0.0f) && ((bot_see_enemy_time + time_in_seconds) < gpGlobals->time)); }
	inline void ResetSeeEnemyTime(void)					{ bot_see_enemy_time = 0.0f; }
	inline void SetDontLookForEnemyTime(float time_in_seconds) { dont_look_for_enemy_time = gpGlobals->time + time_in_seconds; }
	inline bool CanLookForEnemy(void)					{ return (dont_look_for_enemy_time < gpGlobals->time); }
	inline void SetCheckForCloserEnemyTime(float time_in_seconds) { check_for_closer_enemy_time = gpGlobals->time + time_in_seconds; }
	inline bool IsTimeToCheckForCloserEnemy(void)		{ return (check_for_closer_enemy_time < gpGlobals->time); }
	inline void SetWaitForEnemyTime(float time_in_seconds) { wait_for_enemy_time = gpGlobals->time + time_in_seconds; }
	inline float GetWaitForEnemyTime(void)				{ return wait_for_enemy_time; }
	inline bool IsNotWaitingForEnemy(void)				{ return (wait_for_enemy_time < gpGlobals->time); }
	inline bool NotBeenWaitingForEnemyFor(float time_in_seconds) { return ((wait_for_enemy_time + time_in_seconds) < gpGlobals->time); }
	inline void ResetWaitForEnemyTime(void)				{ if (wait_for_enemy_time >= gpGlobals->time) wait_for_enemy_time = gpGlobals->time - 0.1f; }
	inline void SetLastKnownEnemyPosition(Vector foe_origin) { last_known_enemy_position = foe_origin; }
	inline Vector GetLastKnownEnemyPosition(void)		{ return last_known_enemy_position; }
	inline void SetPrevDistanceToEnemy(float foe_distance) { prev_distance_to_enemy = foe_distance; }
	inline float GetPrevDistanceToEnemy(void)			{ return prev_distance_to_enemy; }
	inline void SetBotHideTime(float time_in_seconds)	{ bot_hide_time = gpGlobals->time + time_in_seconds; }
	inline float GetBotHideTime(void)					{ return bot_hide_time; }
	inline bool IsBotNotHiding(void)					{ return (bot_hide_time < gpGlobals->time); }
	inline bool BotNotBeenHidingFor(float time_in_seconds) { return ((bot_hide_time + time_in_seconds) < gpGlobals->time); }
	inline void SetBotReactionTime(float time_in_seconds) { bot_reaction_time = gpGlobals->time + time_in_seconds; }
	inline float GetBotReactionTime(void)				{ return bot_reaction_time; }
	inline bool IsBotReactionTimeOver(void)				{ return (bot_reaction_time < gpGlobals->time); }
	inline void ResetAimDuration()						{ bot_aim_start_time = gpGlobals->time; }
	inline float GetAimDuration()						{ return bot_aim_start_time - gpGlobals->time; }
	inline void AddRecoil(float pitchRecoil)			{ bot_recoil_pitch = fminf(GetRecoil() + pitchRecoil, 90.f); bot_recoil_time = gpGlobals->time; }
	inline float GetRecoil()							{ return fmaxf(0.f, bot_recoil_pitch - pEdict->v.pitch_speed * (gpGlobals->time - bot_recoil_time)); }

	inline void UseWeapon(uWeapon weapon)				{ used_weapon = weapon; }
	inline bool IsUsedWeaponMain(void)					{ return (used_weapon == uWeapon::main); }
	inline bool IsUsedWeaponBackup(void)				{ return (used_weapon == uWeapon::backup); }
	inline bool IsUsedWeaponKnife(void)					{ return (used_weapon == uWeapon::knife); }
	inline bool IsUsedWeaponGrenade(void)				{ return (used_weapon == uWeapon::grenade); }
	inline bool IsUsedWeaponClaymoreMine(void)			{ return (used_weapon == uWeapon::claymoremine); }
	inline bool IsWeaponReady(void)						{ return (weapon_action == W_READY); }
	inline bool IsAllowedToHandleWeapon(float time_in_seconds = 0.0f)
	{
		return ((weapon_action == W_READY) && ((f_shoot_time + time_in_seconds) < gpGlobals->time) && (pEdict->v.movetype != MOVETYPE_FLY));
	}
	inline void SetWeaponStatus(int status)				{ weapon_status |= status; }
	void RemoveWeaponStatus(int status)
	{
		if (weapon_status & status)
			weapon_status &= ~status;
	}
	inline bool IsWeaponStatus(int status)				{ return (weapon_status & status) == status; }
	inline bool IsNoAmmoForMainWeapon(void)				{ return IsWeaponStatus(WS_NOAMMOFORMAIN); }
	inline bool IsNoAmmoForBackupWeapon(void)			{ return IsWeaponStatus(WS_NOAMMOFORBACKUP); }
	inline void ActivateWeaponSecondaryMode(void)		{ SetWeaponStatus(WS_SECONDARYMODEACTIVE); }
	inline void DeactivateWeaponSecondaryMode(void)		{ RemoveWeaponStatus(WS_SECONDARYMODEACTIVE); }
	inline bool IsWeaponSecondaryModeActive(void)		{ return IsWeaponStatus(WS_SECONDARYMODEACTIVE); }
	inline bool IsInSafeDistanceToShoot(Vector origin)	{ return ((origin - pEdict->v.origin).Length() > (GetWeaponSafeRange(main_weapon) / 2.0f)); }
	inline bool IsInSafeDistanceToShoot(float distance) { return (distance > (GetWeaponSafeRange(main_weapon) / 2.0f)); }

	inline bool IsEquippedWithExplosiveCharge(void)		{ return ((bot_tasks & TASK_GOALITEM) && (bot_subtasks & ST_GOALITEM_BOMB)); }

	inline void SetClaymoreMinePlantTime(float time_in_seconds) { claymore_plant_time = f_shoot_time + time_in_seconds; } // based on shoot time to prevent unwanted weapon switching 
	inline bool IsPlantingClaymoreMine(void)			{ return (claymore_plant_time >= gpGlobals->time); }
	inline bool HasPlantedClaymoreMine(void)			{ return (IsWeaponStatus(WS_CLAYMOREGOAL) || IsWeaponStatus(WS_CLAYMORETRIP)); }
	inline void SetGrenadeUseTime(float time_in_seconds) { grenade_use_time = gpGlobals->time + time_in_seconds; }
	inline float GetGrenadeUseTime(void)				{ return grenade_use_time; }
	inline bool IsGrenadeUseTime(void)					{ return (grenade_use_time > gpGlobals->time); }
	inline void SetGrenadesAvailable(void)				{ SetWeaponStatus(WS_GRENADEAVAILABLE); RemoveWeaponStatus(WS_GRENADEDEPLETED); }
	inline bool IsGrenadesAvailable(void)				{ return IsWeaponStatus(WS_GRENADEAVAILABLE); }
	inline void SetGrenadesDepleted(void)				{ SetWeaponStatus(WS_GRENADEDEPLETED); RemoveWeaponStatus(WS_GRENADEAVAILABLE); }
	inline bool IsGrenadesDepleted(void)				{ return IsWeaponStatus(WS_GRENADEDEPLETED); }
	inline void ResetGrenadeActions(void)				{ RemoveWeaponStatus(WS_GRENADEPINPULLED); }
	inline void SetCheckAmmoReservesTime(void)			{ check_ammunition_time = gpGlobals->time; }
	inline void ResetCheckAmmoReservesTime(void)		{ check_ammunition_time = 0.0f; }
	inline bool IsTimeToCheckAmmoReserves(void)			{ return (check_ammunition_time < gpGlobals->time); }
	inline bool NotCheckedAmmoReservesFor(float time_in_seconds) { return ((check_ammunition_time + time_in_seconds) < gpGlobals->time); }
	inline void SetTakeAmmoForMainWeapon(int ammo_count) { take_ammo_for_main_weapon = ammo_count; }
	inline int GetTakeAmmoForMainWeapon(void)			{ return take_ammo_for_main_weapon; }
	inline bool IsTakeAmmoForMainWeapon(void)			{ return (take_ammo_for_main_weapon > 0); }
	inline void SetTakeAmmoForBackupWeapon(int ammo_count) { take_ammo_for_backup_weapon = ammo_count; }
	inline int GetTakeAmmoForBackupWeapon(void)			{ return take_ammo_for_backup_weapon; }
	inline bool IsTakeAmmoForBackupWeapon(void)			{ return (take_ammo_for_backup_weapon > 0); }

	inline void SetFullAutoFireTime(float time_in_seconds) { full_auto_fire_time = gpGlobals->time + time_in_seconds; }
	inline bool IsTimeToFullAutoFire(void)				{ return (full_auto_fire_time >= gpGlobals->time); }
	inline bool NotUsedFullAutoFireFor(float time_in_seconds) { return ((full_auto_fire_time + time_in_seconds) < gpGlobals->time); }
	inline void SetWeaponReloadTime(float time_in_seconds)	{ weapon_reload_time = gpGlobals->time + time_in_seconds; }
	inline float GetWeaponReloadTime(void)				{ return weapon_reload_time; }
	inline bool IsNotReloadingWeapon(void)				{ return (weapon_reload_time < gpGlobals->time); }
	inline void SetBipodDeployTime(float time_in_seconds) { bipod_deploy_time = gpGlobals->time + time_in_seconds; }
	inline bool IsNotDeployingBipod(void)				{ return (bipod_deploy_time < gpGlobals->time); }
	inline void SetBipodYawAngle(float newVal)			{ bipod_yaw_angle = newVal; }
	inline float GetBipodYawAngle(void)					{ return bipod_yaw_angle; }
	inline void SetSnipeTime(float time_in_seconds)		{ snipe_time = gpGlobals->time + time_in_seconds; }
	inline float GetSnipeTime(void)						{ return snipe_time; }
	inline bool IsNotSnipeTime(void)					{ return (snipe_time < gpGlobals->time); }
	inline void SetAdvanceTowardEnemyTime(float time_in_seconds) { advance_toward_enemy_time = gpGlobals->time + time_in_seconds; }
	inline float GetAdvanceTowardEnemyTime(void)		{ return advance_toward_enemy_time; }
	inline bool IsNotAdvancingTowardEnemy(void)			{ return (advance_toward_enemy_time < gpGlobals->time); }
	inline bool NotAdvancedTowardEnemyFor(float time_in_seconds) { return ((advance_toward_enemy_time + time_in_seconds) < gpGlobals->time); }
	inline void SetOverrideAdvanceTime(float time_in_seconds) { override_advance_time = gpGlobals->time + time_in_seconds; }
	inline bool CanOverrideAdvanceTime(void)
	{
		return ((advance_toward_enemy_time < gpGlobals->time) && (override_advance_time < gpGlobals->time) && (IsTask(TASK_DONTMOVEINCOMBAT) == false));
	}
	inline void SetCheckStanceTime(float time_in_seconds) { check_stance_time = gpGlobals->time + time_in_seconds; }
	inline bool IsTimeToCheckStance(void)				{ return (check_stance_time < gpGlobals->time); }
	inline void SetStanceChangeTime(float time_in_seconds) { stance_change_time = gpGlobals->time + time_in_seconds; }
	inline bool IsNotChangingStance(void)				{ return (stance_change_time < gpGlobals->time); }

	inline void SetSpeakTime(float time_in_seconds = 0.0f) { speak_time = gpGlobals->time + time_in_seconds; }
	inline float GetSpeakTime(void)						{ return speak_time; }
	inline bool NotSpokeFor(float time_in_seconds)		{ return ((speak_time + time_in_seconds) < gpGlobals->time); }
	inline voiceCmd GetVoiceCommandToUse(void)			{ return voice_command_to_use; }

	inline bool IsBlindedTime(void)						{ return (blinded_time > gpGlobals->time); }
	inline void SetBlindedTime(float time_in_seconds)	{ blinded_time = gpGlobals->time + time_in_seconds; }
	inline bool IsTimeToSoundsCheck(void)				{ return (sounds_check_time <= gpGlobals->time); }
	inline void SetTimeOfNextSoundsCheck(float time_in_seconds) { sounds_check_time = gpGlobals->time + time_in_seconds; }

	inline float GetPreviousGlobalsTime(void)			{ return prev_globals_time; }
	inline void UpdatePreviousGlobalsTime(void)			{ prev_globals_time = gpGlobals->time; }

	inline edict_t* GetGoal() { return pGoal; }
	inline void SetGoal(edict_t* pGoal) { this->pGoal = pGoal; }
	void IncWaypointPenalty(float penalty);

	bool is_used;
	int respawn_state;
	edict_t *pEdict;
	char name[BOT_NAME_LEN + 1];
	int start_action;
	float kick_time;

// TheFatal - START
	int msecnum;
	float msecdel;
	float msecval;
// TheFatal - END

	int bot_armor;
	int bot_weapons;	// bit map of weapons the bot is carrying

	
	float f_wall_on_right;
	float f_wall_on_left;
	float f_dont_avoid_wall_time;
	
	int curr_wpt_index;
	WptHistory prev_wpt_index;		// a class of functions that manage previously visited waypoints
	AimWptIndex_t Aims;				// a class of functions that manage the aim waypoints

	int  curr_path_index;	// path index the bot is on/following
	int  prev_path_index;	// index of the last path bot was on; to prevent using the same path again and again

	edict_t* pTeamLeader;		// pointer to a teammate that this bot follows

	edict_t *pBotEnemy;			// pointer to current bot enemy
	edict_t *pBotPrevEnemy;		// holds the pointer to current enemy when bot is trying to find another enemy

	float f_shoot_time;			// time the bot last fired (used also in weapon change)

	int main_weapon;		// primary weapon - used most of the time, NO_VAL means not equipped
	int backup_weapon;		// usually handgun, NO_VAL == not equipped
	int melee_weapon;		// knife or spade
	int	grenade_slot;		// holds ID of the grenade the bot has, NO_VAL means not equipped
	int	claymore_slot;		// holds ID of the mine or NO_VAL if not equipped
	int weapon_action;		// holds a flag of current weapon state (e.g. ready to fire or reloaded or changed)

	bot_current_weapon_t current_weapon;  // one current weapon for each bot
	int curr_rgAmmo[MAX_AMMO_SLOTS];	// total ammo amounts (1 array for each bot)

//								NOT USED YET AND WILL PROBABLY NEVER BE USED
//	edict_t *killer_edict;
//	bool	b_bot_say_killed;
//	float	f_bot_say_killed;

	//bool  b_use_button;		// NOT USED
	//float f_use_button_time;	// NOT USED
	//bool  b_lift_moving;		// NOT USED

	//bool	b_use_capture;		// NOT SURE IF NEEDED --- NOT USED --- (PROBABLY FOR FRONTLINE)
	//float	f_use_capture_time;	// NOT SURE IF NEEDED --- NOT USED --- (PROBABLY FOR FRONTLINE)
	//edict_t *pCaptureEdict;	// NOT SURE IF NEEDED --- NOT USED --- (PROBABLY FOR FRONTLINE)

	bool harakiri;	// TRUE when the bot should commit suicide as a result of end game events (ie. no reins)

	// NOTE: by Frank
	// this variable should be changed to a global var., it has nothing to do with certain bot
	// it's only a global event timer same as timer for autobalancing checks
	static float harakiri_moment; //when they should do harakiri.


#ifdef _DEBUG
	bool is_forced;					// debug stuff
	int forced_stance;				// debug stuff

	inline int PrintBehaviour(void) { return bot_behaviour; }
	inline int PrintTasks(void)		{ return bot_tasks; }
	inline int PrintSubTasks(void)	{ return bot_subtasks; }
	inline int PrintNeeds(void)		{ return bot_needs; }
	inline int PrintFASkills(void)	{ return bot_fa_skills; }
	inline int PrintBotFlags(void)	{ return bot_flags; }

	inline float GetSeeTeamLeaderTime(void) { return bot_see_team_leader_time; }

	inline int PrintWeaponStatus(void) { return weapon_status; }
	inline float GetOverrideAdvanceTime(void) { return override_advance_time; }
	inline float GetCheckStanceTime(void) { return check_stance_time; }
	inline float GetStanceChangeTime(void) { return stance_change_time; }
#endif

private:
	int in_team;
	int bot_class;
	int face_skin;			// face/skin the bot uses (camo paint, asian ...)
	int bot_skill;			// used in rate of fire, search for enemy, etc.
	int aiming_skill;		// used in target off-sets
	int bot_behaviour;		// bit map, determines bots behaviour (checking also main weapon), NOT being cleared at respawn
	int bot_tasks;			// bit map, stores short time tasks (is being cleared at each respawn)
	int bot_subtasks;		// bit map, stores actions based on tasks (cleared at respawn)
	int bot_needs;			// bit map, stores things the bot currently needs, like need ammo etc. (cleared at respawn)
	int	bot_fa_skills;		// bit map of Firearms specific skills the bot has (marks1, arty1, leadership, field med etc.) - not cleared at respawn
	int bot_flags;			// bit map of various flags that need to be preserved even when bot dies and respawns (i.e. NOT cleared at respawn init, cleared only at bot creation init)


	float bot_spawn_time;
	
	int current_health;		// current amount of health points
	int previous_health;	// for bleeding checks
	int amount_of_bandages;	// the ammount of bandages the bot has
	float bandage_time;		// time needed to finish bandaging
	float medic_treat_time;	// the time bot needs to treat the wounded soldier

	float paused_time;						// for additional waiting events like waiting for medic etc.
	float time_to_look_for_ground_items;	// time to check for placed claymores, thrown grenades etc.
	Vector ground_item_pos;					// holds the location of the item (eg. claymore)

	float f_max_speed;
	MoveSpeed prev_movespeed;			// move speed from previous frame (for checking if the bot got stuck)
	MoveSpeed current_movespeed;		// move speed in current frame
	float moved_distance_check_time;	// used to determine moved distance (for checking if the bot got stuck)
	Vector prev_origin;					// used to determine moved distance (for checking if the bot got stuck)
	float dont_move_time;				// bot will not move during this time
	int wander_direction;
	float strafe_time;					// the time at which will the bot be moving sideways
	float strafe_direction;				// 0 = none, negative = left, positive = right
	float dont_check_stuck_time;		// the time for which bot will not check for being stuck
	float got_stuck_time;				// the time when bot got stuck (prevents being stuck for long time - bot will kill self)
	int unstuck_attempts;				// holds the number of times the bot tried to unstuck (mostly just turned to different direction)
	float check_deathfall_time;			// time of the last check for deep pit in front of the bot

	float dont_look_for_waypoint_time;	// allows to ignore all waypoint based navigation
	//int curr_wpt_index;				// NOT READY YET
	Vector curr_wpt_fake_position;		// point inside the range of current waypoint the bot is heading towards (it's randomly generated)
	float prev_distance_to_curr_wpt;	// used to check whether the bot ran past current waypoint
	float time_to_reach_curr_wpt;		// used to see if current waypoint is unreachable
	float time_to_face_waypoint;		// time to face current/some waypoint (ie to get the waypoint in front of bot)
	float wpt_action_time;				// time the bot needs to do specific waypoint action like take ammo from ammobox
	float bot_wait_time;				// time taken from current waypoint the bot will use to wait (camp) at that location
	edict_t* pGameEntity;				// pointer to an item bot is trying to use (eg. a mounted gun) or pickup or destroy
	float time_to_face_game_entity;		// time the bot needs to turn directly to the item or specific coordinates in the game world, because it's used even for facing the point in space
	Vector point_in_space;				// allows the bot remember certain point of interest in the map/game world (eg. the origin of door entity so that he can do various checks in order to pass through)
	int curr_aim_index;					// index of the aim waypoint the bot is currently aiming at (watching)
	float time_to_keep_current_aim;		// time the bot is aiming at current aim waypoint
	float time_of_adjusting_the_aim;	// time when the bot started turning to current aim waypoint
	int targeting_the_aim_stop;			// safety stop for a case the bot isn't able to face current aim waypoint
	float duckjump_time;				// the time bot will keep crouched stance in order to successfully finish the duck jump
	LadderDir ladder_use_direction;		// the direction the bot will use the ladder (climb it up or down)
	float start_ladder_time;			// the time when bot got on the ladder (when started climbing it)
	bool reached_end_of_ladder;			// true when the bot reached the waypoint at the end of the ladder
	float parachute_use_time;			// to check if the parachute was used or should be used
	float go_prone_time;				// time needed to correctly finish going to/resuming from proned position

	int patrol_path_waypoint;			// holds index of the last visited waypoint on a PATROL path in order to return to it after combat

	float bot_see_team_leader_time;		// the time bot can see his team leader ... if being part of a fire team

	float bot_see_enemy_time;			// the time when bot see his enemy (i.e. traceline reached enemy entity)
	float dont_look_for_enemy_time;		// the time when bot will not look for enemy
	float check_for_closer_enemy_time;	// the time when bot will check whether there isn't any closer enemy than current one
	float wait_for_enemy_time;			// the time bot will be waiting in combat whether his enemy becomes visible again
	Vector last_known_enemy_position;	// last known enemy position is used to keep looking to that direction when bot lost direct visibily of this enemy
	float prev_distance_to_enemy;		// previous distance to enemy to check if enemy is moving
	float bot_hide_time;				// the time when bot will try to stay hidden in combat (usually crouched)
	float bot_reaction_time;			// delay between spotting the enemy and starting to fight back (i.e. bot won't shoot at the enemy during this time)
	float bot_aim_start_time;			// when the bot started aiming at the current enemy
	float bot_recoil_pitch;				// last applied pitch recoil
	float bot_recoil_time;				// when recoil was applied

	uWeapon used_weapon;				// weapon that is being used: none, main, backup, knife, grenade, claymoremine
	int weapon_status;					// bitmap of current weapon status (e.g. check weapon, mount silencer, etc.)

	float claymore_plant_time;			// the period of time in which the bot has to set claymore mine
	float grenade_use_time;				// for handling bot grenade usage
	float check_ammunition_time;		// the time when bot checked his ammo reserves
	int take_ammo_for_main_weapon;		// number of magazines for main weapon that bot needs to take from ammobox (grenades for attached GL is added too)
	int take_ammo_for_backup_weapon;	// number of magazines for backup weapon that bot needs to take from ammobox

	//float shoot_time;					// NOT READY YET
	float full_auto_fire_time;			// the time when bot keeps fire button pressed
	float weapon_reload_time;			// time needed to reload current weapon
	float bipod_deploy_time;			// time needed to deploy (or fold) bipod
	float bipod_yaw_angle;				// holds the yaw angle (v_angle.y) at the moment bot deployed the bipod, which allows applying max yaw limit for bipod and prevents unlimited turning
	float snipe_time;					// the time when bot doesn't move towards enemy while shooting at him (a must for certain weapons like sniper rifles or machine guns)
	float advance_toward_enemy_time;	// the time when bot moves towards his enemy in combat
	float override_advance_time;		// time to override advance time to allow bot to move even if not time to do so, used when the bot is out of effective range for current weapon
	float check_stance_time;			// time to check whether the bot can change stance (standing/crouched/prone) and keep direct visibility to current enemy in combat
	float stance_change_time;			// the time when bot last changed his stance

	float speak_time;					// the time when bot used one of radio & voice commands
	voiceCmd voice_command_to_use;		// holds the radio or voice or hand signal command the bot has to use
	float text_message_time;			// the time when bot used say or say_team command
	botSay previous_message;			// holds the last message bot said

	float blinded_time;
	float sounds_check_time;			// time of the sound check (e.g. sound of footsteps of possible enemy)


	float prev_globals_time;		// holds previous gpGlobals->time

	edict_t* pGoal;
};

extern bot_t *bots;

typedef struct
{
	char name[BOT_NAME_LEN+1];		// the name
	bool is_used;					// is already used or is still free
} botname_t;

typedef struct
{
	int		iId;					// the weapon ID value
	float	min_safe_distance;		// 0 - no minimum, but sniper rifles, M79, grenades and m16/ak74 (for the attached M203/GP25) should use it
	float	max_effective_distance;	// 9999 - no maximum
} bot_weapon_select_t;

extern bot_weapon_select_t bot_weapon_select[MAX_WEAPONS];

typedef struct
{
	int		iId;									// the weapon ID value
	float	primary_base_delay;						// base delay + random(min, max) -> time of the next shot
	float	primary_min_delay[BOT_SKILL_LEVELS];
	float	primary_max_delay[BOT_SKILL_LEVELS];
} bot_fire_delay_t;

extern bot_fire_delay_t bot_fire_delay[MAX_WEAPONS];

typedef struct
{
	float x_axis;
	float y_axis;
	float z_axis;
	float x_axis_sniper;			// option to make bots more accurate when they are using the scope on the weapon
	float y_axis_sniper;
	float z_axis_sniper;
} bot_target_offset_t;

extern bot_target_offset_t bot_target_offset[BOT_SKILL_LEVELS];

extern float gLastFrameTime;

// define some function prototypes...
BOOL ClientConnect(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128]);
void ClientKill(edict_t* pEntity);
void ClientPutInServer(edict_t* pEntity);
void ClientCommand(edict_t* pEntity);

void FakeClientCommand(edict_t* pBot, const char* arg1, const char* arg2, const char* arg3);
void UTIL_BuildFileName(char* filename, char* arg1, char* arg2, bool separator);
void UTIL_BuildFileName(char* filename, char* arg1, char* arg2, char* arg3);
bool UTIL_IsFile(const char* filename);
void UTIL_StringFromBuffer(char* string, const char* buffer, int left_bracket_char, int right_bracket_char);

const char* Cmd_Args(void);
const char* Cmd_Argv(int argc);
int Cmd_Argc(void);

#endif // BOT_H

