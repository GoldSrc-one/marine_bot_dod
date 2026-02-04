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
// bot_weapons.h
// 
//////////////////////////////////////////////////////////////////////////////////////////////// 

#ifndef BOT_WEAPONS_H
#define BOT_WEAPONS_H

// weapon ID values for DOD
#define WEAPON_AMERKNIFE			1
#define WEAPON_GERKNIFE				2
#define WEAPON_COLT					3
#define WEAPON_LUGER				4
#define WEAPON_GARAND				5
#define WEAPON_SCOPEDKAR			6
#define WEAPON_THOMPSON				7
#define WEAPON_MP44					8
#define WEAPON_SPRING				9
#define WEAPON_KAR					10
#define WEAPON_BAR					11
#define WEAPON_MP40					12
#define WEAPON_HANDGRENADE			13
#define WEAPON_STICKGRENADE			14

#define WEAPON_MG42					17
#define WEAPON_30CAL				18
#define WEAPON_SPADE				19
#define WEAPON_M1CARBINE			20
#define WEAPON_MG34					21
#define WEAPON_GREASEGUN			22
#define WEAPON_FG42					23
#define WEAPON_K43					24
#define WEAPON_ENFIELD				25
#define WEAPON_STEN					26
#define WEAPON_BREN					27
#define WEAPON_WEBLEY				28
#define WEAPON_BAZOOKA				29
#define WEAPON_PSCHRECK				30
#define WEAPON_PIAT					31

typedef struct
{
	int iAmmo1;		// ammo index for primary ammo
	int iAmmo1Max;	// max primary ammo
	int iAmmo2;		// ammo index for secondary ammo
	int iAmmo2Max;	// max secondary ammo
	int iSlot;			// HUD slot (0 based)		- UNSURE
	int iPosition;		// slot position			- UNSURE
	int iId;		// weapon ID
	int iDunno;		// dunno
	int iFlags;		// most probably clip size
} bot_weapon_t;

extern int dod_weapon_amerknife;
extern int dod_weapon_gerknife;
extern int dod_weapon_colt;
extern int dod_weapon_luger;
extern int dod_weapon_garand;
extern int dod_weapon_scopedkar;
extern int dod_weapon_thompson;
extern int dod_weapon_mp44;
extern int dod_weapon_spring;
extern int dod_weapon_kar;
extern int dod_weapon_bar;
extern int dod_weapon_mp40;
extern int dod_weapon_handgrenade;
extern int dod_weapon_stickgrenade;

extern int dod_weapon_mg42;
extern int dod_weapon_30cal;
extern int dod_weapon_spade;
extern int dod_weapon_m1carbine;
extern int dod_weapon_mg34;
extern int dod_weapon_greasegun;
extern int dod_weapon_fg42;
extern int dod_weapon_k43;
extern int dod_weapon_enfield;
extern int dod_weapon_sten;
extern int dod_weapon_bren;
extern int dod_weapon_webley;
extern int dod_weapon_bazooka;
extern int dod_weapon_pschreck;
extern int dod_weapon_piat;


extern char* weapon_name[];

#endif // BOT_WEAPONS_H

