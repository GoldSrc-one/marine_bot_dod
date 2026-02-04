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
// bot_start.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "bot_config.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "bot_weapons.h"
#ifdef DEBUG
#include "console_output.h"
#endif // DEBUG


// bot_start functions prototypes
void BotFinishWeaponSpawning(bot_t *pBot);
inline void BotSetBehaviour(bot_t* pBot);


/*
* sets some important bot spawn/respawn variables and leads the bot through VGUI menus
*/
void bot_t::BotStartGame()
{
	// handle MOTD selection menu										DOESN'T seem to work, probably gets overridden by team select right away, doesn't matter bot can join game anyway
	if (start_action == MSG_VGUI_MOTD_WINDOW)
	{
		start_action = MSG_VGUI_IDLE;  // switch back to idle

		FakeClientCommand(pEdict, "VModEnable", "0", NULL);	// just press "okay"

		return;
	}

	// handle team selection menu
	if (start_action == MSG_VGUI_TEAM_SELECT)
	{
		start_action = MSG_VGUI_IDLE;

		if (IsBotFlag(BF_RESPAWN_TRY_IT_AGAIN))
		{
			ALERT(at_console, "Joining the other team due to uneven teams!\n");

			// switch the team
			if (IsBotTeam(teamONE.GetTeamId()))
				SetBotTeam(teamTWO.GetTeamId());
			else
				SetBotTeam(teamONE.GetTeamId());

			RemoveBotFlag(BF_RESPAWN_TRY_IT_AGAIN);
		}

		// was the team NOT specified...
		if ((IsBotTeam(teamONE.GetTeamId()) == false) && (IsBotTeam(teamTWO.GetTeamId()) == false))
		{
			char nation[16]{};
			bool name_found = false;

			// try to get the team from the name bot already picked
			if (pEdict->v.netname)
			{
				strcpy(nation, GetNationFromBotName(STRING(pEdict->v.netname)));

				// check for german names first, they are on every map
				if (strcmp(nation, "german") == 0)
				{
					name_found = true;
					SetBotTeam(teamTWO.GetTeamId());
				}
				else if ((strcmp(nation, "american") == 0) || (strcmp(nation, "british") == 0))
				{
					name_found = true;
					SetBotTeam(teamONE.GetTeamId());
				}
			}

			// everything failed?
			if (name_found == false)
				SetBotTeam(RANDOM_LONG(1, 2));// then generate it randomly
		}

		// select the team the bot wishes to join
		if (IsBotTeam(teamONE.GetTeamId()))
		{
			FakeClientCommand(pEdict, "jointeam", teamONE.GetTeamIdAsString(), NULL);
		}
		else
		{
			FakeClientCommand(pEdict, "jointeam", teamTWO.GetTeamIdAsString(), NULL);
		}

#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@
		char dmsg[126];
		sprintf(dmsg, "***bot reached end of TEAM selection menu (botTeam=%d)\n", GetBotTeam());
		conOutput.Print(NULL, dmsg, MType::msg_null);
#endif

		return;
	}

	if (start_action == MSG_VGUI_CLASS_SELECT_US)
	{
		start_action = MSG_VGUI_IDLE;	// switch back to idle

		if ((GetBotClass() < 1) || (GetBotClass() > 8))
			SetBotClass(RANDOM_LONG(1, 8));

		// select the class the bot wishes to use
		if (GetBotClass() == 1)
		{
			// RIFLEMAN
			FakeClientCommand(pEdict, "cls_garand", NULL, NULL);

			main_weapon = dod_weapon_garand;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 2)
		{
			// STAFF SERGEANT
			FakeClientCommand(pEdict, "cls_carbine", NULL, NULL);

			main_weapon = dod_weapon_m1carbine;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 3)
		{
			// MASTER SERGEANT
			FakeClientCommand(pEdict, "cls_tommy", NULL, NULL);

			main_weapon = dod_weapon_thompson;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 4)
		{
			// SERGEANT
			FakeClientCommand(pEdict, "cls_grease", NULL, NULL);

			main_weapon = dod_weapon_greasegun;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 5)
		{
			// SNIPER
			FakeClientCommand(pEdict, "cls_spring", NULL, NULL);

			main_weapon = dod_weapon_spring;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
		}
		else if (GetBotClass() == 6)
		{
			// SUPPORT INFANTRY
			FakeClientCommand(pEdict, "cls_bar", NULL, NULL);

			main_weapon = dod_weapon_bar;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 7)
		{
			// MACHINE GUNNER
			FakeClientCommand(pEdict, "cls_30cal", NULL, NULL);

			main_weapon = dod_weapon_30cal;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
		}
		else if (GetBotClass() == 8)
		{
			// BAZOOKA
			FakeClientCommand(pEdict, "cls_bazooka", NULL, NULL);

			main_weapon = dod_weapon_bazooka;
			backup_weapon = dod_weapon_colt;
			melee_weapon = dod_weapon_amerknife;
		}

		// init weapon & stuff settings
		BotFinishWeaponSpawning(this);

		// bot has now joined the game (doesn't need to be started)
		RemoveBotFlag(BF_NOT_JOINED_GAME);

#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@
		char dmsg[126];
		sprintf(dmsg, "***bot reached end of class selection menu (mainW=%d backupW=%d grenade=%d melee=%d)\n", main_weapon, backup_weapon, grenade_slot, melee_weapon);
		conOutput.Print(NULL, dmsg, MType::msg_null);
#endif

		return;
	}

	if (start_action == MSG_VGUI_CLASS_SELECT_AXIS)
	{
		start_action = MSG_VGUI_IDLE;  // switch back to idle

		if ((GetBotClass() < 1) || (GetBotClass() > 8))
			SetBotClass(RANDOM_LONG(1, 8));

		if (GetBotClass() == 1)
		{
			// GRENADIER
			FakeClientCommand(pEdict, "cls_k98", NULL, NULL);

			main_weapon = dod_weapon_kar;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 2)
		{
			// STOSSTRUPPE
			FakeClientCommand(pEdict, "cls_k43", NULL, NULL);

			main_weapon = dod_weapon_k43;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 3)
		{
			// UNTEROFFIZIER
			FakeClientCommand(pEdict, "cls_mp40", NULL, NULL);

			main_weapon = dod_weapon_mp40;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 4)
		{
			// STURMTRUPPE
			FakeClientCommand(pEdict, "cls_mp44", NULL, NULL);

			main_weapon = dod_weapon_mp44;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 5)
		{
			// SCHARFSHUTZE
			FakeClientCommand(pEdict, "cls_k98s", NULL, NULL);

			main_weapon = dod_weapon_scopedkar;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
		}
		else if (GetBotClass() == 6)
		{
			// MG34-SHUTZE
			FakeClientCommand(pEdict, "cls_mg34", NULL, NULL);

			main_weapon = dod_weapon_mg34;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
		}
		else if (GetBotClass() == 7)
		{
			// MG42-SHUTZE
			FakeClientCommand(pEdict, "cls_mg42", NULL, NULL);

			main_weapon = dod_weapon_mg42;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
		}
		else if (GetBotClass() == 8)
		{
			// PANZERJAGER
			FakeClientCommand(pEdict, "cls_pschreck", NULL, NULL);

			main_weapon = dod_weapon_pschreck;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_spade;
		}

		BotFinishWeaponSpawning(this);
		RemoveBotFlag(BF_NOT_JOINED_GAME);


#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@
		char dmsg[126];
		sprintf(dmsg, "***bot reached end of class selection menu (mainW=%d backupW=%d grenade=%d melee=%d)\n", main_weapon, backup_weapon, grenade_slot, melee_weapon);
		conOutput.Print(NULL, dmsg, MType::msg_null);
#endif

		return;
	}

	if (start_action == MSG_VGUI_CLASS_SELECT_AXIS_PARA)
	{
		start_action = MSG_VGUI_IDLE;  // switch back to idle

		if ((GetBotClass() < 1) || (GetBotClass() > 10))
			SetBotClass(RANDOM_LONG(1, 10));

		if (GetBotClass() == 1)
		{
			// GRENADIER
			FakeClientCommand(pEdict, "cls_k98", NULL, NULL);

			main_weapon = dod_weapon_kar;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 2)
		{
			// STOSSTRUPPE
			FakeClientCommand(pEdict, "cls_k43", NULL, NULL);

			main_weapon = dod_weapon_k43;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 3)
		{
			// UNTEROFFIZIER
			FakeClientCommand(pEdict, "cls_mp40", NULL, NULL);

			main_weapon = dod_weapon_mp40;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 4)
		{
			// STURMTRUPPE
			FakeClientCommand(pEdict, "cls_mp44", NULL, NULL);

			main_weapon = dod_weapon_mp44;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
			grenade_slot = dod_weapon_stickgrenade;
		}
		else if (GetBotClass() == 5)
		{
			// SCHARFSHUTZE
			FakeClientCommand(pEdict, "cls_k98s", NULL, NULL);

			main_weapon = dod_weapon_scopedkar;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
		}
		else if (GetBotClass() == 6)
		{
			// FG42-ZWEIBEIN
			FakeClientCommand(pEdict, "cls_fg42", NULL, NULL);

			main_weapon = dod_weapon_fg42;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
			grenade_slot = dod_weapon_stickgrenade;

			// we must set the behaviour right here, because this weapon is shared between two different classes
			SetBehaviour(COMMON);
		}
		else if (GetBotClass() == 7)
		{
			// FG42-ZIELFERNROHR
			FakeClientCommand(pEdict, "cls_fg42_s", NULL, NULL);

			main_weapon = dod_weapon_fg42;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
			grenade_slot = dod_weapon_stickgrenade;

			SetBehaviour(SNIPER);
		}
		else if (GetBotClass() == 8)
		{
			// MG34-SHUTZE
			FakeClientCommand(pEdict, "cls_mg34", NULL, NULL);

			main_weapon = dod_weapon_mg34;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
		}
		else if (GetBotClass() == 9)
		{
			// MG42-SHUTZE
			FakeClientCommand(pEdict, "cls_mg42", NULL, NULL);

			main_weapon = dod_weapon_mg42;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
		}
		else if (GetBotClass() == 10)
		{
			// PANZERJAGER
			FakeClientCommand(pEdict, "cls_pschreck", NULL, NULL);

			main_weapon = dod_weapon_pschreck;
			backup_weapon = dod_weapon_luger;
			melee_weapon = dod_weapon_gerknife;
		}

		BotFinishWeaponSpawning(this);
		RemoveBotFlag(BF_NOT_JOINED_GAME);


#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@
		char dmsg[126];
		sprintf(dmsg, "***bot reached end of class selection menu (mainW=%d backupW=%d grenade=%d melee=%d)\n", main_weapon, backup_weapon, grenade_slot, melee_weapon);
		conOutput.Print(NULL, dmsg, MType::msg_null);
#endif

		return;
	}

	if (start_action == MSG_VGUI_CLASS_SELECT_BRITS)
	{
		start_action = MSG_VGUI_IDLE;  // switch back to idle

		if ((GetBotClass() < 1) || (GetBotClass() > 5))
			SetBotClass(RANDOM_LONG(1, 5));

		if (bot_class == 1)
		{
			// RIFLEMAN
			FakeClientCommand(pEdict, "cls_enfield", NULL, NULL);

			main_weapon = dod_weapon_enfield;
			backup_weapon = dod_weapon_webley;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;

			SetBehaviour(COMMON);
		}
		else if (GetBotClass() == 2)
		{
			// SERGEANT MAJOR
			FakeClientCommand(pEdict, "cls_sten", NULL, NULL);

			main_weapon = dod_weapon_sten;
			backup_weapon = dod_weapon_webley;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 3)
		{
			// MARKSMAN
			FakeClientCommand(pEdict, "cls_enfields", NULL, NULL);

			main_weapon = dod_weapon_enfield;
			backup_weapon = dod_weapon_webley;
			melee_weapon = dod_weapon_amerknife;

			SetBehaviour(SNIPER);
		}
		else if (GetBotClass() == 4)
		{
			// GUNNER
			FakeClientCommand(pEdict, "cls_bren", NULL, NULL);

			main_weapon = dod_weapon_bren;
			backup_weapon = dod_weapon_webley;
			melee_weapon = dod_weapon_amerknife;
			grenade_slot = dod_weapon_handgrenade;
		}
		else if (GetBotClass() == 5)
		{
			// PIAT
			FakeClientCommand(pEdict, "cls_piat", NULL, NULL);

			main_weapon = dod_weapon_piat;
			backup_weapon = dod_weapon_webley;
			melee_weapon = dod_weapon_amerknife;
		}

		BotFinishWeaponSpawning(this);
		RemoveBotFlag(BF_NOT_JOINED_GAME);


#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@
		char dmsg[126];
		sprintf(dmsg, "***bot reached end of class selection menu (mainW=%d backupW=%d grenade=%d melee=%d)\n", main_weapon, backup_weapon, grenade_slot, melee_weapon);
		conOutput.Print(NULL, dmsg, MType::msg_null);
#endif

		return;
	}
}

/*
* inits weapon slots and calls other methods to finalize the spawn
*/
void BotFinishWeaponSpawning(bot_t *pBot)
{
	// assign the behaviour type and weapon usage only when the class selection was successful (ie. we'll skip this when the bot hit the class limit for this game)
	if (pBot->IsBotFlag(BF_RESPAWN_TRY_IT_AGAIN) == false)
	{

#ifdef _DEBUG
		ALERT(at_console, "FinishWeaponSpawning() called\n");
#endif

		// see which weapons are available to the bot and use the best one
		BotSetWeaponsUsage(pBot);

		// set the behaviour now (bot already has weapon)
		if ((pBot->IsBehaviour(STANDARD) == false) && (pBot->IsBehaviour(ATTACKER) == false) && (pBot->IsBehaviour(DEFENDER) == false))
			BotSetBehaviour(pBot);
	}
	// if the bot hit the class limit then we must remove the two behaviour types (related to the two specific weapons) that get assigned within class selection menu
	else
	{
		pBot->RemoveBehaviour(COMMON);
		pBot->RemoveBehaviour(SNIPER);
	}
}

/*
* sets which weapons will bot use or be able to use once he enters the game or respawns
* also sets ammunition flags based on available weapons
*/
void BotSetWeaponsUsage(bot_t* pBot)
{
	// bot has both weapons (main as well as backup) so use main
	if ((pBot->main_weapon != NO_VAL) && (pBot->backup_weapon != NO_VAL))
	{
		pBot->UseWeapon(uWeapon::main);
	}
	// bot has just the main weapon and no backup weapon so use main
	else if ((pBot->main_weapon != NO_VAL) && (pBot->backup_weapon == NO_VAL))
	{
		pBot->UseWeapon(uWeapon::main);
		pBot->SetWeaponStatus(WS_NOAMMOFORBACKUP);
	}
	// bot has no main weapon, but has at least the backup weapon so use it
	else if ((pBot->main_weapon == NO_VAL) && (pBot->backup_weapon != NO_VAL))
	{
		pBot->UseWeapon(uWeapon::backup);
		pBot->SetWeaponStatus(WS_NOAMMOFORMAIN);
	}
	// bot doesn't have any weapon except for the knife so use the knife
	else if ((pBot->main_weapon == NO_VAL) && (pBot->backup_weapon == NO_VAL))
	{
		pBot->UseWeapon(uWeapon::knife);
		pBot->SetWeaponStatus(WS_NOAMMOFORMAIN);
		pBot->SetWeaponStatus(WS_NOAMMOFORBACKUP);
	}

	// make bot gunners and grenadiers (ie. equipped with grenade launcher) use backup weapon while moving around, because their main weapon isn't good for sudden close encounters
	if ((pBot->backup_weapon != NO_VAL) && (IsMachinegun(pBot->main_weapon) || IsRPG(pBot->main_weapon)))
		pBot->UseWeapon(uWeapon::backup);

	// because not every class gets the grenades at spawn we tell the bot he depleted all grenades by default then the first check for ammo will make grenades available for any classes that has them
	// this is crucial for DoD, because this mod doesn't call Weapon Pickup function during player spawn so it's impossible to set grenade availability there
	pBot->SetGrenadesDepleted();
}

/*
* sets the best behaviour type based on main weapon
*/
inline void BotSetBehaviour(bot_t* pBot)
{
	char behaviour_byte1[32], behaviour_byte2[32];

	// set additional behaviour (for path system)
	// if there isn't any behaviour set yet (as a preset from certain classes that share the same weapon) then set it now
	if ((pBot->IsBehaviour(SNIPER) == false) && (pBot->IsBehaviour(MGUNNER) == false) && (pBot->IsBehaviour(CQUARTER) == false) && (pBot->IsBehaviour(COMMON) == false) &&
		(pBot->IsBehaviour(AASPEC) == false))
	{
		// v.playerclass isn't probably valid when this function gets called so we should stick to the preset behaviour
		if (IsSniperRifle(pBot->main_weapon, pBot->pEdict->v.playerclass))
			pBot->SetBehaviour(SNIPER);

		else if (IsMachinegun(pBot->main_weapon))
			pBot->SetBehaviour(MGUNNER);

		else if (IsSMG(pBot->main_weapon))
			pBot->SetBehaviour(CQUARTER);

		else if (IsRPG(pBot->main_weapon))
			pBot->SetBehaviour(AASPEC);

		else
			pBot->SetBehaviour(COMMON);
	}

	// set main behaviour
	// SNIPER RIFLES
	if (pBot->IsBehaviour(SNIPER))
	{
		strcpy(behaviour_byte2, "sniper");

		// in 25% of the time behave normally
		if (RANDOM_LONG(1, 100) > 75)
			pBot->SetBehaviour(STANDARD);
		// otherwise behave defensively
		else
			pBot->SetBehaviour(DEFENDER);
	}
	// MACHINEGUNS
	else if (pBot->IsBehaviour(MGUNNER))
	{
		strcpy(behaviour_byte2,"mgunner");

		// in 25% of the time be aggressive
		if (RANDOM_LONG(1, 100) > 75)
			pBot->SetBehaviour(ATTACKER);
		else
		{
			// in another 25% of the time behave normally
			if (RANDOM_LONG(1, 100) > 75)
				pBot->SetBehaviour(STANDARD);
			// otherwise behave defensively
			else
				pBot->SetBehaviour(DEFENDER);
		}
	}
	// CLOSE QUARTER WEAPONS
	else if (pBot->IsBehaviour(CQUARTER))
	{
		strcpy(behaviour_byte2,"close_quarter");

		// in 75% of the time be aggressive
		if (RANDOM_LONG(1, 100) > 25)
			pBot->SetBehaviour(ATTACKER);
		// otherwise behave normally
		else
			pBot->SetBehaviour(STANDARD);
	}
	// ANTI-ARMOR WEAPONS
	else if (pBot->IsBehaviour(AASPEC))
	{
		strcpy(behaviour_byte2, "anti-armor_specialist");

		// in 25% of the time be aggressive
		if (RANDOM_LONG(1, 100) > 75)
			pBot->SetBehaviour(ATTACKER);
		// otherwise behave normally
		else
			pBot->SetBehaviour(STANDARD);
	}
	// ALL OTHER WEAPONS
	else if (pBot->IsBehaviour(COMMON))
	{
		strcpy(behaviour_byte2,"common_soldier");

		int chance = RANDOM_LONG(1, 100);

		// common soldiers behave 33 on 33 on 33
		if (chance > 66)
			pBot->SetBehaviour(ATTACKER);
		else if (chance > 33)
			pBot->SetBehaviour(STANDARD);
		else
			pBot->SetBehaviour(DEFENDER);
	}

	if (pBot->IsBehaviour(ATTACKER))
		strcpy(behaviour_byte1,"attacker");
	else if (pBot->IsBehaviour(DEFENDER))
		strcpy(behaviour_byte1,"defender");
	else if (pBot->IsBehaviour(STANDARD))
		strcpy(behaviour_byte1,"standard");
	else
		strcpy(behaviour_byte1,"unknown");

#ifdef _DEBUG
	ALERT(at_console, "***behaviour: %s - <%s>\n", behaviour_byte1, behaviour_byte2);
#endif

	return;
}