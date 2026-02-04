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
// client_commands.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

// define constants used to sound confirmation
#define SND_DONE		0
#define SND_FAILED		1


const int val_not_set = 9999;		// used in console input functions
const int cmd_name_length = 32;		// max length of the command name


class console_input_t
{
public:
	console_input_t();
	void Reset(void);
	bool IsCommand(const char* the_command, const char* command_name, const char* alt_cmd_name = NULL);
	bool IsCommand(const char* the_command, const char* command_name, const char* alt_cmd_name1, const char* alt_cmd_name2);
	//bool IsValidBooleanValue(const char* the_argument);		UNUSED
	
	bool IsValidFloatValue(const char* the_argument, float range_from, float range_to, float unique_val = (float) val_not_set);
	bool IsValidIntegerValue(const char* the_argument, int range_from, int range_to, int unique_val = val_not_set);
	bool IsValidKeyWord(const char* the_argument, bool allow_missing_argument = false);
	bool IsValidArrayIndex(const char* the_argument, int array_size);
	bool IsValidPathIndex(const char* the_argument, bool allow_missing_argument = false, const char* unique_keyword = NULL);
	bool IsValidWaypointIndex(const char* the_argument, bool allow_missing_argument = false, const char* unique_keyword = NULL);
	bool IsValidTeam(const char* the_argument, bool allow_numbers = true, bool allow_keyword_both = true);

	inline bool IsTeamOne(void) { return (strcmp(keyword, teamONE.GetTeamName()) == 0); }
	inline bool IsTeamTwo(void) { return (strcmp(keyword, teamTWO.GetTeamName()) == 0); }
	inline bool IsTeamBoth(void) { return (strcmp(keyword, "both") == 0); }

	inline char* GetCmdName(void) { return used_cmd_name; }
	inline void SetCmdName(const char* newName) { strcpy(used_cmd_name, newName); }
	void SetRange(float from, float to);
	bool IsInRange(float value);
	//bool IsOutOfRange(float value);							UNUSED

	inline char* GetKeyWord(void) { return keyword; }
	inline void SetKeyWord(const char* newKey) { strcpy(keyword, newKey); }
	inline void ResetKeyWord(void) { keyword[0] = '\0'; }

	inline void SetMissingArgument(void) { missing_argument = true; }
	inline bool IsMissingArgument(void) { return missing_argument; }

	void ProcessWaypointInput(const char* arg1, const char* arg2);
	
	inline float GetFloatValue(void) { return arg_value; }
	inline int GetIntegerValue(void) { return (int)arg_value; }
	
	inline int GetValidIndex(void) { return valid_index; }
	int PrintValidIndex(void);
	inline void SetValidIndex(int newIndex) { valid_index = newIndex; }
	// sets NO_VAL ie. -1
	inline void ResetValidIndex(void) { valid_index = NO_VAL; }

	inline edict_t* GetCommandInvoker(void) { return pCmdInvoker; }
	inline void SetCommandInvoker(edict_t* pEdict) { pCmdInvoker = pEdict; }
	inline void ResetCommandInvoker(void) { pCmdInvoker = NULL; }

private:
	char used_cmd_name[cmd_name_length];
	char keyword[cmd_name_length];
	bool missing_argument;
	float range_from;
	float range_to;
	float arg_value;
	int valid_index;
	edict_t* pCmdInvoker;
};

extern console_input_t conInput;


bool CustomClientCommands(edict_t* pEntity, const char* pcmd, const char* arg1, const char* arg2, const char* arg3, const char* arg4, const char* arg5);
void PlaySoundConfirmation(edict_t* pEntity, int msg_type);
void MBMenuSystem(edict_t* pEntity, const char* arg1);
void KickBotCommand(edict_t* pEntity, const char* arg1);
void KillBotCommand(edict_t* pEntity, const char* arg1);
void RandomSkillCommand(edict_t* pEntity, const char* arg1);
void SpawnSkillCommand(edict_t* pEntity, const char* arg1);
void SetBotSkillCommand(edict_t* pEntity, const char* arg1);
void BotSkillUpCommand(edict_t* pEntity, const char* arg1);
void BotSkillDownCommand(edict_t* pEntity, const char* arg1);
void SetAimSkillCommand(edict_t* pEntity, const char* arg1);
void SetReactionTimeCommand(edict_t* pEntity, const char* arg1);
void RangeLimitCommand(edict_t* pEntity, const char* arg1);
void SetWaypointDirectoryCommand(edict_t* pEntity, const char* arg1);
void DontSpeakCommand(edict_t* pEntity, const char* arg1);
void DontChatCommand(edict_t* pEntity, const char* arg1);
void DontChatToBotsCommand(edict_t* pEntity, const char* arg1);
void MeleeOnlyCommand(edict_t* pEntity, const char* arg1);
void ChangeStarPositionsCommand(edict_t* pEntity, const char* arg1);
void ChangeCaptureAreasCommand(edict_t* pEntity, const char* arg1);

#endif // !CLIENT_COMMANDS_H