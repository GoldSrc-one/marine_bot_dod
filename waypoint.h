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
// waypoint.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef WAYPOINT_H
#define WAYPOINT_H

#include <limits.h>

#define MAX_TRIGGERS	8		// max triggers we can use

#define MAX_WAYPOINTS 4096		// the maximum of waypoints for a map

#define MIN_WPT_DIST 80.0f		// used in auto waypointing - this is the radius around any previously placed waypoint where no other waypoint is added
#define MAX_WPT_DIST 400.0f		// max distance between two waypoints bot can head toward to									??? does the path system actually utilize this? ???

#define WPT_RANGE 50.0f			// default waypoint range ie radius around the waypoint (if this gets changed to other value then reset custom default waypoint range in botmanager.h file needs to be updated too)
#define WPT_RANGE_SMALL	20.0f	// waypoint range used for some specific waypoints (eg. ammobox, door, jump, use) and also used to check whether the bot has to slow down (ie. < range small)
constexpr float WPT_BANDAGE_RANGE = 30.0f;	// default range for waypoint when there is the bandage item detected at it
constexpr float WPT_CROSS_RANGE = WPT_RANGE * 3.0f;	// default range for cross waypoint

#define MIN_WPT_PRIOR 0			// used to test valid waypoint priority
#define MAX_WPT_PRIOR 5			// used to test valid waypoint priority
constexpr int NON_ZERO_WPT_PRIORITY = -1;		// used to validate waypoint priority that isn't zero (ie. priorities 1..5 will pass such validation, but 0 will not), function needs to allow such checking else it won't validate

constexpr float MAX_AIM_WPT_DIST = 100.0f;		// max distance of Aim waypoint to connect it to waypoint with "wait" time


// define the IDs for trigger event messages
// the order cannot be changed otherwise conversion routines would return wrong data
enum class WaypointTriggersIDs
{
	trigger_none = 0,
	trigger1,
	trigger2,
	trigger3,
	trigger4,
	trigger5,
	trigger6,
	trigger7,
	trigger8
};
typedef WaypointTriggersIDs TriggerId;

// trigger event structure that's being saved into the waypoint files
struct trigger_event_t
{
	TriggerId		name;			// it's ID
	char			message[256];
};
typedef trigger_event_t TRIGGER_EVENT;

// trigger event array
extern TRIGGER_EVENT trigger_events[MAX_TRIGGERS];

// old trigger event structure (used for conversions)
// (this was used in version 6)
struct old_trigger_event_t
{
	TriggerId		name;			// it's ID
	bool			used;
	char			message[256];
	bool			triggered;
};
typedef old_trigger_event_t TRIGGER_EVENT_OLD;

// trigger event structure that's holding actual game state of the triggers
// this one is not being saved into the waypoint files, because these variables do change during the game so we don't need to save them
class trigger_event_gamestate_t
{
public:
	trigger_event_gamestate_t();
	void Init(int index);
	inline void SetName(TriggerId new_name) { name = new_name; }
	inline TriggerId GetName(void) { return name; }
	inline void SetUsed(bool flag) { used = flag; }
	inline bool GetUsed(void) { return used; }
	inline void SetTriggered(bool flag) { triggered = flag; }
	inline bool GetTriggered(void) { return triggered; }
	inline void SetTime(void) { time = gpGlobals->time; }
	inline float GetTime(void) { return time; }

private:
	TriggerId		name;			// it's ID - the glue to the trigger structure
	bool			used;			// is this trigger being used?
	bool			triggered;		// did this event happen?
	float			time;			// the time when this event happened
};

// array of current game state of the trigger events
extern trigger_event_gamestate_t trigger_gamestate[MAX_TRIGGERS];


// waypoint types ie. fake waypoint flags
// to handle the problem with not enough space for all waypoint flags we (will/would) need
// these are NOT being saved to waypoint/paths files
enum class WaypointTypes
{
	scrapped_flagtype = 0,	// in case we would need to scrap any waypoint flag type
	normal,
	crouch,
	prone,
	jump,
	duckjump,
	sprint,
	ammobox,
	bandage,
	door,			// doors that open automatically when you are nearby
	dooruse,		// doors that you need to use to open
	ladder,
	cover,
	use,			// use a button, pull a lever etc.
	parachute,		// is used as a check gate before the jump, not as a position where the parachute item is located
	sniper,			// hold the position when in combat e.g. behind sandbag or on a roof
	claymore,		// claymore mine
	shoot,			// to shoot at a breakable object or FA specific search & destroy object
	aim,			// special marker for a direction that the bot will be aiming at
	pushpoint,		// checkpoint location on maps with the ps_ prefix
	trigger,		// allows switching between two sets of priority settings based on given game events
	roadblock,		// allows tracelining area between two of these to determine reachability
	cross,			// special marker for a crossroad ie. where one route splits into multiple directions or vice versa
	goback,			// turn back and return to the start of this path
	deleted,		// deleted waypoint ie. doesn't exist on the map at the moment, but can be reused when adding new waypoint to the map
};
typedef WaypointTypes WptT;


// waypoints.flags constants (waypoint types)
#define W_FL_STD			(1<<0)	// normal/standing wpt (also used when autowaypointing)
#define W_FL_CROUCH			(1<<1)	// crouch wpt (must crouch to reach)
#define W_FL_PRONE			(1<<2)	// prone wpt (laying down)
#define W_FL_JUMP			(1<<3)	// jump wpt
#define W_FL_DUCKJUMP		(1<<4)	// jump wpt (press crouch and jump together)
#define W_FL_SPRINT			(1<<5)	// sprint wpt
#define W_FL_AMMOBOX		(1<<6)	// ammobox wpt
#define W_FL_BANDAGE		(1<<7)	// bandages laying here										-- NOT READY YET --						// was unused before version 6
#define W_FL_DOOR			(1<<8)	// door wpt (wait for door to open)
#define W_FL_DOORUSE		(1<<9)	// door wpt (press "use" to open)
#define W_FL_LADDER			(1<<10)	// ladder wpt
#define W_FL_COVER			(1<<11)	// cover waypoint											-- NOT READY YET --						// new addition with version 6
#define W_FL_USE			(1<<12)	// use wpt (press "use" to activate that)
#define W_FL_CHUTE			(1<<13)	// parachute jump (check if have chute if not turn back)
#define W_FL_SNIPER			(1<<14)	// sniper wpt (don't move while in combat)

#define W_FL_AVOID			(1<<15)	//															NOT READY & NOT SURE ABOUT IT
#define W_FL_MINE			(1<<16)	// plant a claymore mine here

#define W_FL_TEAMMATE		(1<<17) // wait here for teammate									-- NOT READY YET --
#define W_FL_FIRE			(1<<18) // press weapon trigger (ie fire)
#define W_FL_AIMING			(1<<19)	// aim to this direction
#define W_FL_PUSHPOINT		(1<<20) // used to handle push point based map objectives													// new addition with version 6
#define W_FL_TRIGGER		(1<<21) // used to handle objective based map objectives (ie. field guns on obj_bocage)						// new addition with version 6
#define W_FL_ROADBLOCK		(1<<22)	// used to handle spots that are unreachable for period of time (eg. avalanche pit on ps_coldwar)	// new addition with version 8

#define W_FL_CROSS			(1<<29)	// crossroad wpt (bot don't head directly to it, it's only a mark to start search code)
#define W_FL_GOBACK			(1<<30)	// turn back and continue in opposite direction (ie from this wpt back to start)

#define W_FL_DELETED		(1<<31)	// used by waypoint allocation code


//#define W_FL_LIFT        (1<<25)  /* PREVIOUS BOTMANS SYSTEM - PROBABLY USELESS - wait for lift to be down before approaching this waypoint - newly with version 6 */


#define WAYPOINT_VERSION 8	// current waypoint version, must be changed when doing bigger
							// changes in waypointing system to prevent various bugs like missing wpts, messed waypoint data etc.

// holds the version number of last waypoint system version
// used to detect known old system to allow MB convert waypoints and paths
// last version was 7 which was used in mb0.91b up to 0.95b (including 0.95.1 & 0.95.2 test versions)
#define OLD_WAYPOINT_VERSION WAYPOINT_VERSION - 1

// define the structure for waypoints
struct waypoint_t
{
	int			flags;			// bitmap of waypoint types (ammobox, crouch, ladder etc.)
	int			red_priority;	// 0-5 where 1 means highest priority for red team (0 means no priority i.e. bot ignores this waypoint as a whole or some of its flags)
	float		red_time;		// the time for which red bot will wait at this waypoint
	int			blue_priority;	// 0-5 where 1 means highest priority for blue team
	float		blue_time;		// the time for which blue bot will wait at this waypoint
	int			trigger_red_priority;	// 0-5 for red team, but it's used only when the trigger flag is set on this waypoint and the trigger event on has been fired
	int			trigger_blue_priority;	// the same as above, but for blue team
	TriggerId	trigger_event_on;		// the event that will switch this waypoint (if it has the trigger flag set) to the use of the trigger priorities
	TriggerId	trigger_event_off;		// the event that will switch this waypoint (if it has the trigger flag set) back to the use of standard priorities
	float		range;			// the circle around this waypoint that the bot considers as that he actually reached this waypoint
	Vector		origin;			// the true location of this waypoint in 3D space (i.e. on the map)
};
typedef waypoint_t WAYPOINT;

// previous waypoint structure (used in cases we changed the structure itself)
// (this was used in version 6 and version 7 too)
struct old_waypoint_t
{
	int		flags;			// jump, crouch, button, lift, flag, ammo, etc.
	int		red_priority;	// 0-5 where 1 have highest priority for red team (0 - no priority, bot ingnores this waypoint)
	float	red_time;		// time the bot wait at this wpt for red team
	int		blue_priority;	// 0-5 where 1 have highest priority for blue team
	float	blue_time;		// time the bot wait at this wpt for blue team
	int		trigger_red_priority;	// 0-5 is being used only when trigger flag is set for this wpt and is trigger event
	int		trigger_blue_priority;
	int		trigger_event_on;		// the event that's triggering the waypoint with trigger flag
	int		trigger_event_off;		// the event that's resetting the waypoint with trigger flag back to normal
	float	range;			// circle around wpt where bot detects this waypoint
	Vector	origin;			// location of this waypoint in 3D space
};
typedef old_waypoint_t OLD_WAYPOINT;

// define the waypoint file header structure
struct waypoint_file_header_t
{
	char filetype[8];			// should be "FAM_bot\0"
	int  waypoint_file_version;	// holds the waypoint system version (from above) so that we always read correct data (or start conversion if possible)
	int  waypoint_file_flags;	// not currently used
	int  number_of_waypoints;
	char mapname[32];			// name of map for these waypoints
	char author[32];			// author signature - can't be modified
	char modified_by[32];		// signature of guy who modify them - could be changed
};
typedef waypoint_file_header_t WAYPOINT_HDR;


/*		NOT USED

// define the raw waypoint file header structure
typedef struct {
   char filetype[8];  // should be "FAM_bot\0"
   int  waypoint_file_version;
   int  transfer_flags;
   int  transfer_priority;
   int  transfer_time;
   int  transfer_class;
   int  number_of_waypoints;
   char mapname[32];  // name of map for these waypoints
   char author[32];
   char modified_by[32];
} RAW_FILE_HDR;

*/

// array of all waypoints for this map
extern WAYPOINT waypoints[MAX_WAYPOINTS];

// number of waypoints currently in use
extern int num_waypoints;

extern float waypoint_penalty[MAX_WAYPOINTS];

#define MAX_W_PATHS			512		// the maximum number of path for a map
#define AUTOADD_DISTANCE	10.0	// player must be this close to add a waypoint to the path

constexpr int LINKEDLIST_LOOPS_THRESHOLD = MAX_WAYPOINTS;	// to prevent infinite loops while working with paths linked list, equals to max waypoints count


// constants used to highlight certain paths
// they must be negative because positive values are standard path indexs needed when we highlight one specific path
#define HIGHLIGHT_DISABLED		-1	// turned off
#define HIGHLIGHT_TEAMONE			-2	// team one - allies
#define HIGHLIGHT_TEAMTWO			-3	// team two - axis
#define HIGHLIGHT_ONEWAY		-4
#define HIGHLIGHT_SNIPER		-5
#define HIGHLIGHT_MGUNNER		-6
#define HIGHLIGHT_ANTIARMOR		-7


// path types ie. fake path flags
// to handle the problem with not enough space for all the flags we (will/would) need
// these are NOT being saved to waypoint/paths files
enum class WaypointPathTypes
{
	scrapped_flagtype = 0,	// in case we would need to scrap any path flag type
	both_teams,
	team_one,				// red team in Firearms
	team_two,				// blue team in Firearms
	one_way,				// start->end only
	two_way,				// start->end as well as end->start
	patrol_cycle,			// start->end->start->end... (leaving this path is randomized)
	all_classes,
	sniper_class,
	mgunner_class,
	antiarmor_class,		// ie. bazooka, piat or panzerschreck
	path_defender,			// bot with defender behaviour - NOT USED & NOT SURE ABOUT IT
	ammo_tag,				// automatically tagged by the system when there's an ammobox on that path
	bandages_tag,			// NOT USED YET! - will be automatically tagged by the system
	roadblocked_tag,		// automatically tagged by the system when there're roadblock waypoints on the path
	goal_team_one_tag,		// automatically tagged by the system when there's a pushpoint waypoint on the path
	goal_team_two_tag,		// automatically tagged by the system when there's a pushpoint waypoint on the path
	avoid_far_enemy,		// bot will attack only enemy that's within his weapon range and will hold the position in combat
	ignore_the_enemy,		// bot will ignore most enemies except for those that are really close
	carry_goal_item,		// a preferred path whenever bot carries a briefcase or any goal item
	goal_explosives_tag,	// automatically tagged by the system when there's a claymore waypoint set as a goal (priority == 1) on the path
	cover_tag,				// UNDONE
	teammate_tag,			// UNDONE
	path_danger,			// NOT USED & NOT SURE ABOUT IT - more or less replaced by roadblocked tag
	path_turret,			// NOT USED & NOT SURE ABOUT IT
};
typedef WaypointPathTypes PathT;


// team flag constants
#define P_FL_TEAM_NO			(1<<0)		// both teams use it
#define P_FL_TEAM_RED			(1<<1)
#define P_FL_TEAM_BLUE			(1<<2)

// way/direction constants
#define P_FL_WAY_ONE			(1<<5)		// one way (start-end)
#define P_FL_WAY_TWO			(1<<6)		// both way (start-end as well as end-start)
#define P_FL_WAY_PATROL			(1<<7)		// cycle (start-end-start)

// class flag constants
#define P_FL_CLASS_ALL			(1<<10)		// all bots use it
#define P_FL_CLASS_SNIPER		(1<<11)		// only bots using sniper rifle use it
#define P_FL_CLASS_MGUNNER		(1<<12)
#define P_FL_CLASS_ANTIARMOR	(1<<13)		// only bots using the anti-armor launcher use it (ie. bazooka, piat and panzerschreck classes)
#define P_FL_CLASS_DEFENDER		(1<<14)		// the bot whose behaviour is defender				// NOT USED & NOT SURE ABOUT IT

// misc flag constant
#define P_FL_MISC_AMMO			(1<<20)		// there's an ammobox waypoint on this path
#define P_FL_MISC_BANDAGES		(1<<21)		// there's a bandages waypoint on this path
#define P_FL_MISC_ROADBLOCKED	(1<<22)		// there're roadblock waypoints on this path and there's something between them blocking the way so path is unavailable
#define P_FL_MISC_GOAL_RED		(1<<23)		// there's a pushpoint waypoint on this path, red team needs to get there
#define P_FL_MISC_GOAL_BLUE		(1<<24)		// there's a pushpoint waypoint on this path, blue team needs to get there
#define P_FL_MISC_AVOID			(1<<25)		// the bot won't attack enemy that's outside current weapon effective range
#define P_FL_MISC_IGNORE		(1<<26)		// the bot will ignore enemy unless the enemy is right next to the bot
#define P_FL_MISC_GITEM			(1<<27)		// the bot will prefer this path whenever carrying the goal/captured item (case, flag, ammo ... whatever)
#define P_FL_MISC_GEXPLOSIVES	(1<<28)		// the bot will prefer this path when there's a claymore waypoint set as goal (ie priority == 1) and bot has the claymore mine
#define P_FL_MISC_COVER			(1<<29)		// -= UNDONE =-													// new addition with version 8
#define P_FL_MISC_TEAMMATE		(1<<30)		// -= UNDONE =-													// new addition with version 8

//#define P_FL_MISC_DANGER		(1<<28)		// there's a danger of death fall somewhere on this path		NOT USED & NOT SURE ABOUT IT (replaced by road blocked)
//#define P_FL_MISC_TURRET		(1<<29)		// there's a mounted gun by the use waypoint on this path		NOT USED & NOT SURE ABOUT IT

// used to let the path algorithm know that bot should do a turn and continue in opposite direction
#define PATH_TURNBACK (W_FL_GOBACK | W_FL_AMMOBOX | W_FL_USE)

// define the structure for waypoint paths
struct w_path_t
{
	int wpt_index;		// index of current waypoint
	int flags;
	struct w_path_t* prev;	// previous node in linked list
	struct w_path_t* next;	// next node in linked list
};
typedef w_path_t W_PATH;

// previous path structure (used in cases we changed the structure itself)
// (this was used in version 6 and version 7 too)
struct old_w_path_t
{
	int wpt_index;		// index of current waypoint
	int flags;
	struct old_w_path_t* prev;	// previous node in linked list
	struct old_w_path_t* next;	// next node in linked list
};
typedef old_w_path_t OLD_W_PATH;

// define waypoint paths file header structure
struct waypoint_paths_file_header_t
{
	char filetype[8];				// should be "FAM_bot\0"
	int  waypoint_file_version;		// must be the same value as in waypoint file header
	int  waypoint_flag;				// this holds the same value as 'waypoint_file_header_type.number_of_waypoints' so that we can check for data synchronization between both files 
	int  number_of_paths;
	char mapname[32];				// name of map for these waypoints
	char author[32];				// author signature - can't be modified
	char modified_by[32];			// signature of guy who modify them - could be changed
};
typedef waypoint_paths_file_header_t PATH_HDR;

// array of all paths for this map
extern W_PATH *w_paths[MAX_W_PATHS];

// number of paths currently in use
extern int num_w_paths;


struct waypoint_value_t
{
	int wpt_index;			// the index of the waypoint
	int wpt_value;			// its value in terms of priority/usefulness over other waypoints
};
typedef waypoint_value_t WAYPOINT_VALUE;

struct w_path_value_t
{
	int path_index;
	int path_value;
};
typedef w_path_value_t PATH_VALUE;

class path_value_manager_t
{
public:
	inline void SetPathValueArraySize(int newVal) { array_size = newVal; }
	void InitPathValueArray(PATH_VALUE* path_value_array);
	void SetValue(bot_t* pBot, int path_index, PATH_VALUE* path_value_array);
	inline bool IsAnyPathAvailable(PATH_VALUE* path_value_array) { return (path_value_array[0].path_index != NO_VAL); }
	bool IsAnyValuedPath(PATH_VALUE* path_value_array);
	int GetMostValuedPath(PATH_VALUE* path_value_array);
	int GetRandomPath(PATH_VALUE* path_value_array);
	inline int GetAvailablePathsCount(void) { return available_paths_count; }
	inline int GetValuedPathsCount(void) { return valued_paths_count; }

private:
	void ResetArraySlot(PATH_VALUE* path_value_array);

	int array_size;
	int available_paths_count;
	int valued_paths_count;
};

// class of variables and functions used to display waypoints, paths and connections
// basically tools to allow proper waypoints debugging 
class waypointsbrowser_t
{
public:
	waypointsbrowser_t();
	void ResetOnMapChange(void);
	inline void SetShowWaypoints(bool newVal) { waypoints_on = newVal; }
	inline bool IsShowWaypoints(void) { return waypoints_on; }
	inline void ResetShowWaypoints(void) { waypoints_on = false; }
	inline void SetShowPaths(bool newVal) { paths_on = newVal; }
	inline bool IsShowPaths(void) { return paths_on; }
	inline void ResetShowPaths(void) { paths_on = false; }
	inline void SetCheckAims(bool newVal) { check_aim_connections = newVal; }
	inline bool IsCheckAims(void) { return check_aim_connections; }
	inline void ResetCheckAims(void) { check_aim_connections = false; }
	inline void SetCheckCross(bool newVal) { check_cross_connections = newVal; }
	inline bool IsCheckCross(void) { return check_cross_connections; }
	inline void ResetCheckCross(void) { check_cross_connections = false; }
	inline void SetCheckRanges(bool newVal) { check_waypoints_ranges = newVal; }
	inline bool IsCheckRanges(void) { return check_waypoints_ranges; }
	inline void ResetCheckRanges(void) { check_waypoints_ranges = false; }
	inline void SetCheckShoot(bool newVal) { check_shoot_objects = newVal; }
	inline bool IsCheckShoot(void) { return check_shoot_objects; }
	inline void ResetCheckShoot(void) { check_shoot_objects = false; }
	inline void SetAutoWaypointing(bool newVal) { auto_waypointing = newVal; }
	inline bool IsAutoWaypointing(void) { return auto_waypointing; }
	inline void ResetAutoWaypointing(void) { auto_waypointing = false; }
	inline void SetAutoAddToPath(bool newVal) { auto_add_to_path = newVal; }
	inline bool IsAutoAddToPath(void) { return auto_add_to_path; }
	inline void ResetAutoAddToPath(void) { auto_add_to_path = false; }
	inline void SetWaypointsDrawDistance(float newVal) { waypoints_draw_distance = newVal; }
	inline float GetWaypointsDrawDistance(void) { return waypoints_draw_distance; }
	inline void ResetWaypointsDrawDistance(void) { waypoints_draw_distance = 800.0f; }
	inline void SetWaypointsDisplayTime(float newVal) { waypoints_display_time = newVal; }
	inline float GetWaypointsDisplayTime(void) { return waypoints_display_time; }
	inline void ResetWaypointsDisplayTime(void) { waypoints_display_time = 0.5f; }
	inline void SetPathsDisplayTime(float newVal) { paths_display_time = newVal; }
	inline float GetPathsDisplayTime(void) { return paths_display_time; }
	inline void ResetPathsDisplayTime(void) { paths_display_time = 0.5f; }
	inline void SetAutoWaypointingDistance(float newVal) { auto_waypointing_distance = newVal; }
	inline float GetAutoWaypointingDistance(void) { return auto_waypointing_distance; }
	inline void ResetAutoWaypointingDistance(void) { auto_waypointing_distance = 200.0f; }
	inline void SetCompassIndex(int newVal) { waypoint_compass_index = newVal; }
	inline int GetCompassIndex(void) { return waypoint_compass_index; }
	inline void ResetCompassIndex(void) { waypoint_compass_index = NO_VAL; }
	inline void SetPathToHighlight(int newVal) { path_to_highlight = newVal; }
	inline int GetPathToHighlight(void) { return path_to_highlight; }
	inline void ResetPathToHighlight(void) { path_to_highlight = HIGHLIGHT_DISABLED; }
	inline bool IsPathToHighlightAPathIndex(void) { return ((path_to_highlight >= 0) && (path_to_highlight < num_w_paths)); }

	bool ShowCompass(edict_t* pEntity, const char* arg2);
	void PrintWaypointInfo(edict_t* pEntity, const char* arg2, const char* arg3);
	void PrintTriggerWaypointInfo(edict_t* pEntity, const char* arg2);
	void PrintAllWaypoints(edict_t* pEntity);
	bool PrintPathInfo(edict_t* pEntity, int path_index);
	bool PrintWholePath(edict_t* pEntity, int path_index);
	bool PrintAllPaths(edict_t* pEntity, int wpt_index = NO_VAL);
	inline bool IsHUDTextLineOverLengthLimit(const char* the_text) { return ((int)strlen(the_text) >= externals.GetHUDTextLineLength()); }

	int GetWaypointsSystemVersion(void);
	void PrintWaypointsAuthors(char* author, char* modified_by);


private:
	bool waypoints_on;				// to show waypoints
	bool paths_on;					// to show paths
	bool check_aim_connections;		// to show connections between waypoints with wait time and nearby aim waypoints
	bool check_cross_connections;	// to show connections between cross waypoint and all waypoints within its range
	bool check_waypoints_ranges;	// shows waypoint range using two thin beams intersecting in the middle of the waypoint
	bool check_shoot_objects;		// to show connections between shoot waypoint and all breakable objects within its reach
	bool auto_waypointing;			// allows to automatically add waypoints as the user moves around
	bool auto_add_to_path;			// allows to automatically add "touched" waypoint to current path
	float waypoints_draw_distance;	// max distance for a waypoint to show on screen, distance between user and waypoint
	float waypoints_display_time;	// how often will be the waypoints beams (plus the aim, cross and range) updated a.k.a. redrawn on the screen
	float paths_display_time;		// how often will be the paths beams updated a.k.a. redrawn on the screen
	float auto_waypointing_distance;// the distance between any two waypoints when auto waypointing
	int waypoint_compass_index;		// index of the waypoint the user is looking for
	int path_to_highlight;			// allows displaying of only certain paths or one specific path
};

// class of variables and functions used to display waypoints, paths and connections (ie. tools to allow proper waypoint debugging)
extern waypointsbrowser_t wptser;

// class of variables and functions used to handle console output especially for the waypoint repair functions
class waypoint_console_output_manager_t
{
public:
	waypoint_console_output_manager_t();
	inline int GetErrorCount(void) { return error_counter; }
	inline void IncErrorCount(void) { error_counter++; }
	inline void ResetErrorCount(void) { error_counter = 0; }
	inline int GetWarningCount(void) { return warning_counter; }
	inline void IncWarningCount(void) { warning_counter++; }
	inline void ResetWarningCount(void) { warning_counter = 0; }
	inline int GetAmountOfFoundIssues(void) { return error_counter + warning_counter; }
	void ResetAmountOfFoundIssues(void);
	inline int GetPrintedLines(void) { return lines_to_be_printed_on_screen; }
	inline void IncPrintedLines(int newLines = 1) { lines_to_be_printed_on_screen = lines_to_be_printed_on_screen + newLines; }
	inline void ResetPrintedLines(void) { lines_to_be_printed_on_screen = 0; }
	inline bool CanStillPrintIt(void) { return (lines_to_be_printed_on_screen <= max_lines_on_screen); }
	inline bool IsPrintErrorsOnly(void) { return print_errors_only; }
	inline void SetPrintErrorsOnly(void) { print_errors_only = true; }
	inline void ResetPrintErrorsOnly(void) { print_errors_only = false; }
	inline bool IsOverrideCounterReset(void) { return override_default_counter_reset; }
	inline void SetOverrideCounterReset(void) { override_default_counter_reset = true; }
	inline void ResetOverrideCounterReset(void) { override_default_counter_reset = false; }
	void ResetCounters(void);
	void AddError(int LinesOfText = 1);
	void AddWarning(int LinesOfText = 1);
	void ProcessIt(const char* message, bool log_in_file = false);

private:
	int error_counter;
	int warning_counter;
	int lines_to_be_printed_on_screen;		// holds current amount of lines of text that will be printed on screen
	const int max_lines_on_screen = 22;		// the max number of lines of text that can be printed on screen to keep all lines visible/readable (especially for WON HL console)
	bool print_errors_only;					// outputs only the errors when enabled
	bool override_default_counter_reset;	// prevents counters reset at the start of each repair function
};

// class of variables and functions used to handle console output especially for the waypoint repair functions
extern waypoint_console_output_manager_t wptoutputer;


// class of functions responsible for adding/removing waypoint to/from map, altering their properties and saving/loading them to/from HDD
class waypoint_editing_functions_t
{
public:
	void InitAll(void);
	int Add(edict_t* pEntity, const char* wpt_type);
	WptT AddType(const Vector position, WptT wpt_type);
	void Delete(edict_t* pEntity);
	int ChangeType(edict_t* pEntity, const char* new_type, int wpt_index = NO_VAL);
	int ChangePriority(edict_t* pEntity, const char* set_this_priority, const char* for_team);
	float ChangeTime(edict_t* pEntity, const char* set_this_time, const char* for_team);
	float ChangeRange(edict_t* pEntity, const char* set_this_range);
	float ChangeRangeByConstantValue(edict_t* pEntity, const char* the_value, bool decreasing = false);
	bool ChangePosition(edict_t* pEntity, int wpt_index, const char* arg2, const char* arg3);
	int ResetData(edict_t* pEntity, const char* arg1, const char* arg2, const char* arg3, const char* arg4);
	int AddTriggerEvent(const char* trigger_name, const char* trigger_message);
	int DeleteTriggerEvent(const char* trigger_name);
	int ChangeTriggerPriority(edict_t* pEntity, const char* priority, const char* for_team);
	int ConnectTriggerEvent(edict_t* pEntity, const char* trigger_name, const char* state);
	int RemoveTriggerEvent(edict_t* pEntity, const char* state);

	void StartAutoWaypointg(bool switch_on);
	void WipeAll(void);
	bool Subscribe(const char* signature, bool is_it_the_author, bool enforced = false);
	bool AutoSaveWaypoints(void);
	bool SaveWaypoints(const char* custom_filename);
	int LoadWaypoints(edict_t* pEntity, const char* custom_filename);
	bool LoadUnsupportedWaypoints(edict_t* pEntity);
	bool LoadUnsupportedWaypointsVersion6(edict_t* pEntity);
	bool LoadFirearmsWaypoints(edict_t* pEntity, const char* custom_filename);
	void FinalizeWaypointConversion(void);

private:
	void InitThisWaypoint(int wpt_index);
	void DetectBandagesAroundWaypoint(int wpt_index);
};

// class of functions responsible for adding/removing waypoint to/from map, altering their properties and saving/loading them to/from HDD
extern waypoint_editing_functions_t wpteditor;


// class of functions responsible for creating/deleting paths, altering their properties and saving/loading them to/from HDD
class path_editing_functions_t
{
public:
	bool Create(edict_t* pEntity, int wpt_index);
	bool Finish(edict_t* pEntity);
	bool Continue(edict_t* pEntity, int path_index);
	bool Delete(edict_t* pEntity, int path_index);
	bool AddWaypoint(edict_t* pEntity, int wpt_index);
	int InsertWaypoint(const char* new_wpt, const char* to_path, const char* arg4, const char* arg5);
	bool RemoveWaypoint(edict_t* pEntity, int wpt_index, int from_path_index);
	int Split(edict_t* pEntity, const char* this_path, const char* on_wpt);
	bool Reverse(edict_t* pEntity, int path_index);
	int ChangeDirection(edict_t* pEntity, const char* new_value, int path_index);
	int ChangeTeam(edict_t* pEntity, const char* new_value, int path_index);
	int ChangeClass(edict_t* pEntity, const char* new_value, int path_index);
	int ChangeMisc(edict_t* pEntity, const char* new_value, int path_index);
	bool ResetToDefaults(edict_t* pEntity, int path_index);
	void SetAutoTag(PathT path_type, int path_index);
	void ResetAutoTag(PathT path_type, int path_index);

	bool SavePaths(const char* custom_filename);
	int LoadPaths(edict_t* pEntity, const char* custom_filename);
	bool LoadUnsupportedPaths(edict_t* pEntity);
	bool LoadUnsupportedPathsVersion6(edict_t* pEntity);
	bool LoadFirearmsPaths(edict_t* pEntity, const char* custom_filename);

	void FreeAllPaths(void);

private:
	void RemoveExistingDirectionFlags(int path_index);
	void RemoveExistingTeamFlags(int path_index);
	void RemoveExistingClassFlags(int path_index);

};

// class of functions responsible for creating/deleting paths, altering their properties and saving/loading them to/from HDD
extern path_editing_functions_t patheditor;


// class of functions used to checking for various bugs in waypoints and paths, as well as repairing them where possible
class waypoints_and_paths_repair_functions_t
{
public:
	void CheckWaypointsForProblems(bool log_in_file);
	void CheckPathsForProblems(bool log_in_file);

	int RepairInvalidCombinationOfWaypointFlags(int wpt_index, bool repair_it = true, bool log_in_file = false);
	int RepairInvalidCombinationOfWaypointPriorityAndTime(int wpt_index, bool repair_it = true, bool log_in_file = false);
	void RepairInvalidCombinationOfWaypointPriorityAndTime(void);
	float RepairCrossWaypointRange(int wpt_index);
	void RepairCrossWaypointRange(void);
	bool RepairWaypointRangeAndPosition(int wpt_index, edict_t* pEdict, bool dont_move = false);
	void RepairWaypointRangeAndPosition(edict_t* pEdict, bool dont_move = false);
	
	int RepairSniperSpot(int path_index);
	void RepairSniperSpot(void);
	int RepairInvalidPathEnd(int path_index);
	void RepairInvalidPathEnd(void);
	int RepairInvalidPathMerge(int path_index, bool repair_it = true, bool log_in_file = false);
	void RepairInvalidPathMerge(void);

	int DeleteInvalidPaths(bool print_details);
	int ValidatePath(int path_index);
	void ValidatePath(void);

	void UpdatePathStatus(int path_index);

	void SwapTeamsInWaypoints(void);

private:
	bool IsInvalidWaitTime(int wpt_index, bool simplified_report = true, bool log_in_file = false);
	bool IsInvalidCombinationOfWaypointAndPath(int path_index, PathT path_type, WptT waypoint_type);
	int CheckInvalidPathEnd(int path_index, bool log_in_file);

	float SelfControlledCrossWaypointRangeIncrease(int crosswpt_index, int& ignored_wpt);
	float SelfControlledCrossWaypointRangeDecrease(int crosswpt_index, int& ignored_wpt);
	void SelfControlledWaypointReposition(float& the_range, Vector& new_origin, float move_d, float dec_r, bool dont_move, edict_t* pentIgnore);
	
	int FixSniperSpot(int path_index, int wpt_index, bool repair_it = true, bool log_in_file = false);
	int MergePaths(int path1_index = NO_VAL, int path2_index = NO_VAL, bool reverse_order = false);
	int MergePathsInverted(int path1_index = NO_VAL, int path2_index = NO_VAL, bool reverse_order = false);
	int PurifyPath(int path_index);

	void UpdatePathStatusPushpoint(int wpt_index, int path_index);
	void UpdatePathStatusRoadblock(int wpt_index, int path_index, bool& status_lock);
};

// class of functions used to checking for various bugs in waypoints and paths, as well as repairing them where possible
extern waypoints_and_paths_repair_functions_t wptfixer;


// class of functions used to search, compare and get waypoints and paths
class waypoints_and_paths_managing_functions_t
{
public:
	int CountWaypointFlags(int wpt_index);

	int FindAimingAround(int source_waypoint_index);
	int FindWaypointOfTypeAround(const Vector& source_origin, WptT wpt_type, float distance = WPT_RANGE);
	int FindConnectedCross(int source_waypoint_index);
	int FindConnectedCross(const Vector& source_origin);
	int FindConnectedCross(const Vector& source_origin, bool see_through_doors);
	int FindNearestCross(const Vector& source_origin, bool see_through_doors);
	int FindNearestOrdinaryWaypoint(int end_waypoint, int path_index);
	int FindNearestWaypointToPlayer(edict_t* pEntity, float distance = WPT_RANGE, int team = NO_VAL);
	int FindNearestWaypointOfTypeToPlayer(edict_t* pEntity, float range, WptT wpt_type);
	int	FindPath(int wpt_index);

	int FindNewWaypointForBot(bot_t* pBot, int skip_this_index);
	int FindNewWaypointForBotAtPathEnd(bot_t* pBot, int wpt_index);
	int FindNextWaypointOnShortestPath(int startingWaypoint, const Vector& goal);
	int FindNextWaypointForBot(bot_t* pBot);
	int FindAimingWaypointsForBot(bot_t* pBot, int wpt_index);

	float GetDistanceToWaypoint(edict_t* pEntity, int wpt_index);
	float GetDistanceBetweenWaypoints(int wpt1_index = NO_VAL, int wpt2_index = NO_VAL);
	int GetWaypointPriority(int wpt_index, int team);
	int GetTriggerWaypointPriority(int wpt_index, int team);
	float GetWaypointWaitTime(int wpt_index, int team);
	void GetWaypointName(int wpt_index, char* wpt_names);
	WptT GetWaypointTypeFromName(const char* waypoint_name = NULL);
	W_PATH* GetWaypointPointer(int wpt_index, int path_index);
	W_PATH* GetWaypointTypePointer(WptT wpt_type, int path_index);

	void GetPathClass(int path_index, char* the_class);
	int GetPathLength(int path_index);
	int GetPathStart(int path_index);
	int GetPathEnd(int path_index);
	int GetPathNextWaypoint(int wpt_index, int path_index);
	int GetPathPreviousWaypoint(int wpt_index, int path_index);

	bool IsWaypoint(int wpt_index, WptT wpt_type);
	bool IsWaypoint(int wpt_index, WptT wpt_type1, WptT wpt_type2);
	bool IsWaypoint(int wpt_index, WptT wpt_type1, WptT wpt_type2, WptT wpt_type3);
	bool IsWaypointPriority(int wpt_index, int searched_priority);
	bool IsWaypointTypeTeamPriority(int wpt_index, WptT wpt_type, int searched_priority, int for_team);
	bool IsNoRangeWaypoint(int wpt_index);
	bool IsPathWaypointTypeTeamPriority(int path_index, WptT wpt_type, int searched_priority, int for_team);
	bool IsPathWaypointPairTypeTeamPriority(int path_index, WptT wpt_type, int searched_priority, int for_team);
	bool IsWaypointOnPath(int wpt_index, int path_index);
	bool IsWaypointTypeOnPath(WptT wpt_type, int path_index);
	bool IsWaypointAtPathEnd(int wpt_index, int path_index = NO_VAL);
	bool IsWaypointCloserToPathEnd(int wpt_index, int path_index);
	bool IsWaypointTypeNeighbourOnPath(W_PATH* wpt_pointer, WptT wpt_type);
	bool IsPath(int path_index, PathT path_type);
	bool IsPath(int path_index, PathT path_type1, PathT path_type2);
	bool IsPath(int path_index, PathT path_type1, PathT path_type2, PathT path_type3);

	bool IsWaypointAccessibleForThisBot(bot_t* pBot, int wpt_index, bool is_called_at_crosswpt = false);
	bool IsPathAccessibleForThisBot(bot_t* pBot, int path_index);
	inline bool IsRoadblockedPathException(bot_t* pBot, int path_index);
	bool IsPairedWaypointReachableForThisBot(bot_t* pBot, WptT wpt_type);
	bool IsPatrolWaypointReachableForThisBot(bot_t* pBot);
	bool IsPushpointGoalOnPathReachableForThisBot(bot_t* pBot, int path_index);
	bool WasPossiblePathForBotFoundOnWaypoint(bot_t* pBot, int wpt_index);

	int AreBothPathEndsConnectedToOneCrossWaypoint(int wpt_index, int path_index = NO_VAL);
	bool AreBothPathsOfSameType(int path_index1, int path_index2, PathT path_type);
	bool CanSwitchToThisPathDueToTeamLimitingFactors(int checked_path, int restrictive_path);
	bool CanSwitchToThisPathDueToClassLimitingFactors(int checked_path, int restrictive_path);

private:
	inline bool IsValidWaypointIndex(int wpt_index) { return ((wpt_index >= 0) && (wpt_index <= num_waypoints)); };
};

// class of functions used to search, compare and get waypoints and paths
extern waypoints_and_paths_managing_functions_t wptmanager;


class display_beam_t
{
public:
	display_beam_t() { start_point = NO_VAL; end_point = NO_VAL; sprite = NO_VAL; color = g_vecZero; prev = next = NULL; };
	int start_point;
	int end_point;
	int sprite;
	Vector color;
	display_beam_t* prev;
	display_beam_t* next;
};


class display_path_beams_t
{
public:
	display_path_beams_t();
	~display_path_beams_t();
	void AddNewBeam(int beam_start_point, int beam_end_point, int beam_sprite, Vector beam_color);
	int GetMatchingBeamsCount(int beam_start_point, int beam_end_point, int beam_sprite, Vector beam_color);
	bool CanBeDisplayed(int beam_start_point, int beam_end_point, int beam_sprite, Vector beam_color);
	void DrawIt(edict_t* pEdict, Vector beam_start_point, Vector beam_end_point, int beam_sprite, Vector beam_color, float duration);

private:
	display_beam_t* start;
	const int max_shown_beams = 2;		// maximum of same beams shown between two waypoints at one moment
};


bool WaypointMoveWholePath(int path_index, float, int coord);
//bool  WaypointRawLoad(edict_t *pEntity, bool flags, bool priority, bool time, bool class_preference); // NOT USED
//void  WaypointRawSave(bool flags, bool priority, bool time, bool class_preference);	// NOT USED

int FindRightLadderWpt(bot_t* pBot);

void DrawBeam(edict_t *pEntity, Vector start, Vector end, int life, int red, int green, int blue, int speed);

void UpdateWaypointData(void);
void WaypointThink(edict_t* pEntity);

#endif // WAYPOINT_H
