//====== Copyright ?1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_NAILGUN_H
#define TF_WEAPON_NAILGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"

// Client specific.
#ifdef CLIENT_DLL
#define CTFNailGun C_TFNailGun
#endif

//=============================================================================
//
// TF Weapon Syringe gun.
//
class CTFNailGun : public CTFWeaponBaseGun
{
public:

	DECLARE_CLASS( CTFNailGun, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFNailGun() {}
	~CTFNailGun() {}

	virtual void Precache();
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_NAILGUN; }

private:

	CTFNailGun( const CTFNailGun & ) {}
};

#endif // TF_WEAPON_NailGun_H
