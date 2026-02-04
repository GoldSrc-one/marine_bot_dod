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
// bot_client.h
// 
////////////////////////////////////////////////////////////////////////////////////////////////

void BotClient_VGUI(void *p, int bot_index);
void BotClient_WeaponList(void *p, int bot_index);
void BotClient_CurrentWeapon(void *p, int bot_index);
void BotClient_AmmoX(void *p, int bot_index);
void BotClient_AmmoShort(void* p, int bot_index);
void BotClient_AmmoPickup(void *p, int bot_index);
void BotClient_WeaponPickup(void *p, int bot_index);
void BotClient_ItemPickup(void *p, int bot_index);
void BotClient_Health(void *p, int bot_index);
void BotClient_Damage(void *p, int bot_index);
void BotClient_DeathMsg(void *p, int bot_index);
void BotClient_TextMsg(void *p, int bot_index);
void BotClient_FOV(void* p, int bot_index);
void BotClient_ScreenFade(void* p, int bot_index);
void BotClient_MOTD(void* p, int bot_index);
void BotClient_Object(void* p, int bot_index);
void BotClient_ClientAreas(void* p, int bot_index);
void BotClient_ReloadDone(void* p, int bot_index);
void BotClient_HandSignal(void* p, int bot_index);


// messages to all clients

void BotClient_InitObj(void* p, int bot_index);
void BotClient_SetObj(void* p, int bot_index);
void BotClient_PlayersIn(void* p, int bot_index);
void BotClient_RoundState(void* p, int bot_index);
void BotClient_CapMsg(void* p, int bot_index);


void BotClient_FA_HUDMsg(void *p, int bot_index);
