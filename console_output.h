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
// counsole_output.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CONSOLE_OUTPUT_H
#define CONSOLE_OUTPUT_H

// the size of the engine Text Message used in Client Print function, sending more characters only leads to corrupted output, but it doesn't seem to crash the server
#define TEXT_MSG_SIZE 128


enum class MarineBotMessageType
{
	msg_null = 0,
	msg_default,
	msg_info,
	msg_warning,
	msg_error,
	msg_critical_error,
	msg_cfg_passed,
	msg_cfg_failed
};

typedef MarineBotMessageType MType;


enum class console_output_error_message_types_t
{
	inv_mis_arg,//				invalid or missing argument
	inv_arg,//					invalid argument
	mis_arg,//					missing argument
	inv_wpt,//					invalid waypoint index or not allowed there
	no_pth_no_wpt_nrb,//		path doesn't exist or not close to its waypoint
	no_pth,//					path doesn't exist
	no_wpt_nrb,//				no waypoint close enough
	inv_trig_name,//			invalid trigger name
	inv_trig_state,//			invalid trigger state
	inv_team_val,//				invalid team value
};

typedef console_output_error_message_types_t conOutErrMsg;


class console_output_t
{
public:
	console_output_t();
	void Print(edict_t* pEdict, const char* message, MType msg_type = MType::msg_null);
	void Print(edict_t* pEdict, const char* message, const char* var_name, MType msg_type = MType::msg_null);
	void Notify(char* msg);
	void Notify(char* msg, bot_t* pBot, bool ignore_last_msg_filter = false);
	void Notify(char* msg, bool islogging);
	void Notify(char* msg, bool islogging, bot_t* pBot, bool ignore_last_msg_filter = false);
	void PrintToClient(edict_t* pEdict, int msg_destination, const char* message);
	void AppendBeforeNewline(char* text_line, const char* string_to_add);
	void ShortenLineOfText(char* text_line, const char* ending_string, int max_line_len, int num_chars_to_scan);
	void RemoveStringFromBuffer(char* the_buffer, const char* string_to_remove);
	void RemoveStringFromBuffer(char* the_buffer, const char* string_to_remove, int start_at_pos);

	void PrintErrorMessage(conOutErrMsg error_message_type, edict_t* pEdict = NULL, bool disable_sound_confirmation = false);
	//void PrintErrorMessage(bool var, edict_t* pEdict = NULL);	UNUSED
	void PrintErrorMessage(float var, edict_t* pEdict = NULL);
	void PrintErrorMessage(int var, edict_t* pEdict = NULL);

private:
	void EchoConsole(const char* message);
	void LogPrint(char* fmt, ...);
};

extern console_output_t conOutput;

#endif // !CONSOLE_OUTPUT_H
