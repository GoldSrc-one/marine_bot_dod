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
// bot_config.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BOT_COFIG_H
#define BOT_COFIG_H

#include <stdio.h>

void UTIL_StringFromBuffer(char* string, const char* buffer, int left_bracket_char, int right_bracket_char);

// set these two large enough to be able to hold the whole strings you will be reading from the external files else you'll be experiencing crashes
const int ceSize = 64;	// max length of the entry string including the terminating character
const int evSize = 128;	// max length of the value string including the terminating character

class config_t
{
public:
	config_t();
	bool OpenConfigFile(const char* filename);
	void CloseConfigFile(void);
	inline bool IsConfigFile(void) { return (fp != NULL); }
	void Rewind(void);
	void ResetConfigHistory(void);
	bool FindKey(const char* searched_key_name);
	bool FindKeyScope(const char* searched_key_name);
	bool ReadEntry(char* entry_string);
	bool ReadEntryScope(char* entry_string);
	bool ReadValue(char* entry_value);
	bool ReadValueScope(char* entry_value);
	bool ReadValueWithSpace(char* entry_value, int value_length = evSize);
	bool ReadBooleanValue(bool default_value);
	float ReadFloatValue(float default_value);
	int ReadIntegerValue(int default_value);
	void ReadFloatArray(float the_array[], int array_size, float default_value);
	bool GetBooleanCVar(const char* cvar_name, bool default_value);
	float GetFloatCVar(const char* cvar_name, float default_value, float range_from, float range_to, float unique_val = 9999.0f);
	int GetIntegerCVar(const char* cvar_name, int default_value, int range_from, int range_to);
	bool ReadTeam(char* the_team, const char* allowed_value1, const char* allowed_value2);
	bool ReadClass(char* the_class, int range_from, int range_to);
	bool ReadSkin(char* the_skin, int range_from, int range_to);
	bool ReadSkill(char* the_skill, int range_from, int range_to);
	bool ReadCustomClass(char* entry_string, int class_number);
	int CountTheCustomClasses(int class_to_check, int &empty_class_counter);

	void SetEntryName(const char* new_entry_name);
	inline char* GetEntryName(void) { return entry_name; }
	void ResetScope(void);
	void SetScope(const char* string);
	void SetCVarName(const char* new_cvar_name);
	inline char* GetCVarName(void) { return cvar_name; }
	inline bool IsOutOfScope(void) { return out_of_scope; }
	inline void SetOutOfScope(void) { out_of_scope = true; }
	inline bool IsReadError(void) { return read_error; }
	inline void ResetReadError(void) { read_error = false; }
	void SetReadError(void);
	inline bool IsSectionError(void) { return section_error; }
	inline void ResetSectionError(void) { section_error = false; }
	inline void SetSectionError(void) { section_error = true; }
	void CreateErrorMessage(char* message, const char* entry_name);
	inline char* GetErrorMessage(void) { return er_msg; }
	inline bool IsErrorMessage(void) { return (er_msg[0]); }
	inline void ResetErrorMessage(void) { er_msg[0] = 0; }

private:
	FILE* fp;
	char entry_name[ceSize];
	char scope_end[ceSize];
	char cvar_name[ceSize];
	bool out_of_scope;
	bool read_error;
	bool section_error;
	char er_msg[256];
};


extern config_t configFile;

#endif // BOT_COFIG_H