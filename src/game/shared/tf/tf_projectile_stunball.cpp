#include "cbase.h"
#include "tf_projectile_stunball.h"
#include "tf_gamerules.h"
#include "effect_dispatch_data.h"
#include "tf_weapon_bat.h"

#ifdef GAME_DLL
#include "tf_fx.h"
#include "tf_gamestats.h"
#else
#include "particles_new.h"
#endif


#define TF_STUNBALL_MODEL	  "models/weapons/w_models/w_baseball.mdl"
#define TF_STUNBALL_LIFETIME  15.0f

IMPLEMENT_NETWORKCLASS_ALIASED( TFStunBall, DT_TFStunBall )

BEGIN_NETWORK_TABLE( CTFStunBall, DT_TFStunBall )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bCritical ) ),
#else
	SendPropBool( SENDINFO( m_bCritical ) ),
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
BEGIN_DATADESC( CTFStunBall )
END_DATADESC()
#endif

LINK_ENTITY_TO_CLASS( tf_projectile_stunball, CTFStunBall );
PRECACHE_REGISTER( tf_projectile_stunball );

CTFStunBall::CTFStunBall()
{
}

CTFStunBall::~CTFStunBall()
{
#ifdef CLIENT_DLL
	ParticleProp()->StopEmission();
#endif
}

#ifdef GAME_DLL
CTFStunBall *CTFStunBall::Create( CBaseEntity *pWeapon, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseCombatCharacter *pOwner, CBaseEntity *pScorer, const AngularImpulse &angVelocity, const CTFWeaponInfo &weaponInfo )
{
	CTFStunBall *pStunBall = static_cast<CTFStunBall *>( CBaseEntity::CreateNoSpawn( "tf_projectile_stunball", vecOrigin, vecAngles, pOwner ) );

	if ( pStunBall )
	{
		// Set scorer.
		pStunBall->SetScorer( pScorer );

		// Set firing weapon.
		pStunBall->SetLauncher( pWeapon );

		DispatchSpawn( pStunBall );

		pStunBall->InitGrenade( vecVelocity, angVelocity, pOwner, weaponInfo );

		pStunBall->ApplyLocalAngularVelocityImpulse( angVelocity );
	}

	return pStunBall;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFStunBall::Precache( void )
{
	PrecacheModel( TF_STUNBALL_MODEL );
	PrecacheScriptSound( "TFPlayer.StunImpactRange" );
	PrecacheScriptSound( "TFPlayer.StunImpact" );

	PrecacheTeamParticles( "stunballtrail_%s", false );
	PrecacheTeamParticles( "stunballtrail_%s_crit", false );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFStunBall::Spawn( void )
{
	SetModel( TF_STUNBALL_MODEL );
	SetDetonateTimerLength( TF_STUNBALL_LIFETIME );

	BaseClass::Spawn();
	SetTouch( &CTFStunBall::StunBallTouch );

	CreateTrail();

	// Pumpkin Bombs
	AddFlag( FL_GRENADE );

	// Don't collide with anything for a short time so that we never get stuck behind surfaces
	SetCollisionGroup( TFCOLLISION_GROUP_NONE );

	AddSolidFlags( FSOLID_TRIGGER );

	m_flCreationTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::Explode( trace_t *pTrace, int bitsDamageType )
{
	m_takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->fraction != 1.0 )
	{
		SetAbsOrigin( pTrace->endpos + ( pTrace->plane.normal * 1.0f ) );
	}

	CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase * >( m_hLauncher.Get() );

	// Pull out a bit.
	if ( pTrace->fraction != 1.0 )
	{
		SetAbsOrigin( pTrace->endpos + ( pTrace->plane.normal * 1.0f ) );
	}

	// Damage.
	CTFPlayer *pAttacker = dynamic_cast< CTFPlayer * >( GetThrower() );
	CTFPlayer *pPlayer = dynamic_cast< CTFPlayer * >( m_hEnemy.Get() );

	// TODO: check for invuln/dodge/etc.
	if ( pPlayer && pAttacker && pPlayer->GetTeamNumber() != pAttacker->GetTeamNumber() )
	{
		float flAirTime = gpGlobals->curtime - m_flCreationTime;
		Vector vecDir = GetAbsOrigin();
		VectorNormalize( vecDir );

		// Do damage.
		CTakeDamageInfo info( this, pAttacker, pWeapon, GetDamage(), GetDamageType(), TF_DMG_CUSTOM_BASEBALL );
		CalculateBulletDamageForce( &info, pWeapon ? pWeapon->GetTFWpnData().iAmmoType : 0, vecDir, GetAbsOrigin() );
		info.SetReportedPosition( pAttacker ? pAttacker->GetAbsOrigin() : vec3_origin );
		pPlayer->DispatchTraceAttack( info, vecDir, pTrace );
		ApplyMultiDamage();

		if ( flAirTime > 0.2f )
		{

			int iBonus = 1;
			// TODO: Look at these values some more later
			if ( flAirTime > 1.0f )
			{
				// Cap the maximum stun time to 7 seconds
				flAirTime = 1.0f;
				pPlayer->PlayStunSound( pPlayer, "TFPlayer.StunImpactRange" );

				// 2 points for moonshots
				iBonus++;
			}
			else
			{
				pPlayer->PlayStunSound( pPlayer, "TFPlayer.StunImpact" );
			}

			pPlayer->m_Shared.StunPlayer( 7.0f * ( flAirTime ), 0.8f, STUN_CONC, pAttacker );
			pAttacker->SpeakConceptIfAllowed( MP_CONCEPT_STUNNED_TARGET );

			// Bonus points.
			IGameEvent *event_bonus = gameeventmanager->CreateEvent( "player_bonuspoints" );
			if ( event_bonus )
			{
				event_bonus->SetInt( "player_entindex", pPlayer->entindex() );
				event_bonus->SetInt( "source_entindex", pAttacker->entindex() );
				event_bonus->SetInt( "points", iBonus );

				gameeventmanager->FireEvent( event_bonus );
			}
			CTF_GameStats.Event_PlayerAwardBonusPoints( pAttacker, pPlayer, iBonus );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::StunBallTouch( CBaseEntity *pOther )
{
	// Verify a correct "other."
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		return;

	trace_t pTrace;
	Vector velDir = GetAbsVelocity();
	VectorNormalize( velDir );
	Vector vecSpot = GetAbsOrigin() - velDir * 32;
	UTIL_TraceLine( vecSpot, vecSpot + velDir * 64, MASK_SOLID, this, COLLISION_GROUP_NONE, &pTrace );

	CTFPlayer *pPlayer = dynamic_cast< CTFPlayer * >( pOther );

	// Make us solid once we reach our owner
	if ( GetCollisionGroup() == TFCOLLISION_GROUP_NONE )
	{
		if ( pOther == GetThrower() )
			SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

		return;
	}

	// Stun the person we hit
	if ( pPlayer && ( gpGlobals->curtime - m_flCreationTime > 0.2f || GetTeamNumber() != pPlayer->GetTeamNumber() ) )
	{
		if ( !m_bTouched )
		{
			// Save who we hit for calculations
			m_hEnemy = pOther;
			m_hSpriteTrail->SUB_FadeOut();
			Explode( &pTrace, GetDamageType() );

			// Die a little bit after the hit
			SetDetonateTimerLength( 3.0f );
			m_bTouched = true;
		}

		CTFWeaponBase *pWeapon = pPlayer->Weapon_GetWeaponByType( TF_WPN_TYPE_MELEE );
		if ( pWeapon && pWeapon->PickedUpBall( pPlayer ) )
		{
			UTIL_Remove( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::Detonate()
{
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	int otherIndex = !index;
	CBaseEntity *pHitEntity = pEvent->pEntities[otherIndex];

	if ( !pHitEntity )
	{
		return;
	}

	// we've touched a surface
	m_bTouched = true;
	
	// Handle hitting skybox (disappear).
	surfacedata_t *pprops = physprops->GetSurfaceData( pEvent->surfaceProps[otherIndex] );
	if ( pprops->game.material == 'X' )
	{
		// uncomment to destroy ball upon hitting sky brush
		//SetThink( &CTFGrenadePipebombProjectile::SUB_Remove );
		//SetNextThink( gpGlobals->curtime );
		return;
	}

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFStunBall::SetScorer( CBaseEntity *pScorer )
{
	m_Scorer = pScorer;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBasePlayer *CTFStunBall::GetAssistant( void )
{
	return dynamic_cast<CBasePlayer *>( m_Scorer.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::Deflected( CBaseEntity *pDeflectedBy, Vector &vecDir )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		Vector vecOldVelocity, vecVelocity;

		pPhysicsObject->GetVelocity( &vecOldVelocity, NULL );

		float flVel = vecOldVelocity.Length();

		vecVelocity = vecDir;
		vecVelocity *= flVel;
		AngularImpulse angVelocity( ( 600, random->RandomInt( -1200, 1200 ), 0 ) );

		// Now change grenade's direction.
		pPhysicsObject->SetVelocityInstantaneous( &vecVelocity, &angVelocity );
	}

	CBaseCombatCharacter *pBCC = pDeflectedBy->MyCombatCharacterPointer();

	IncremenentDeflected();
	m_hDeflectOwner = pDeflectedBy;
	SetThrower( pBCC );
	ChangeTeam( pDeflectedBy->GetTeamNumber() );

	// Change trail color.
	if ( m_hSpriteTrail.Get() )
	{
		UTIL_Remove( m_hSpriteTrail.Get() );
	}

	CreateTrail();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFStunBall::GetDamageType()
{
	int iDmgType = BaseClass::GetDamageType();

	if ( m_bCritical )
	{
		iDmgType |= DMG_CRITICAL;
	}
	if ( m_iDeflected > 0 )
	{
		iDmgType |= DMG_MINICRITICAL;
	}

	return iDmgType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFStunBall::GetTrailParticleName( void )
{
	return ConstructTeamParticle( "effects/baseballtrail_%s.vmt", GetTeamNumber(), false, g_aTeamNamesShort );
}

// ----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::CreateTrail( void )
{
	CSpriteTrail *pTrail = CSpriteTrail::SpriteTrailCreate( GetTrailParticleName(), GetAbsOrigin(), true );

	if ( pTrail )
	{
		pTrail->FollowEntity( this );
		pTrail->SetTransparency( kRenderTransAlpha, 255, 255, 255, 128, kRenderFxNone );
		pTrail->SetStartWidth( 9.0f );
		pTrail->SetTextureResolution( 0.01f );
		pTrail->SetLifeTime( 0.4f );
		pTrail->TurnOn();

		pTrail->SetContextThink( &CBaseEntity::SUB_Remove, gpGlobals->curtime + 5.0f, "RemoveThink" );

		m_hSpriteTrail.Set( pTrail );
	}
}
#else
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flCreationTime = gpGlobals->curtime;
		CreateTrails();
	}

	if ( m_iOldTeamNum && m_iOldTeamNum != m_iTeamNum )
	{
		ParticleProp()->StopEmission();
		CreateTrails();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStunBall::CreateTrails( void )
{
	if ( IsDormant() )
	return;

	if ( m_bCritical )
	{
		const char *pszEffectName = ConstructTeamParticle( "stunballtrail_%s_crit", GetTeamNumber(), false );
		ParticleProp()->Create( pszEffectName, PATTACH_ABSORIGIN_FOLLOW );
	}
	else
	{
		const char *pszEffectName = ConstructTeamParticle( "stunballtrail_%s", GetTeamNumber(), false );
		ParticleProp()->Create( pszEffectName, PATTACH_ABSORIGIN_FOLLOW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Don't draw if we haven't yet gone past our original spawn point
//-----------------------------------------------------------------------------
int CTFStunBall::DrawModel( int flags )
{
	if ( gpGlobals->curtime - m_flCreationTime < 0.1f )
		return 0;

	return BaseClass::DrawModel( flags );
}
#endif