#include "cbase.h"
#include "asw_weapon_flechette2_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#define CASW_Weapon C_ASW_Weapon
#define CASW_Marine C_ASW_Marine
#define CBasePlayer C_BasePlayer
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "precache_register.h"
#include "c_te_effect_dispatch.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "te_effect_dispatch.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "decals.h"
#include "ammodef.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "episodic/npc_hunter.h"
#include "asw_gamerules.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Flechette2, DT_ASW_Weapon_Flechette2 )

BEGIN_NETWORK_TABLE( CASW_Weapon_Flechette2, DT_ASW_Weapon_Flechette2 )
#ifdef CLIENT_DLL
// recvprops
#else
// sendprops	
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Flechette2 )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_flechette2, CASW_Weapon_Flechette2 );
PRECACHE_WEAPON_REGISTER( asw_weapon_flechette2 );

extern ConVar asw_weapon_max_shooting_distance;

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
extern ConVar sk_hunter_dmg_flechette;
ConVar rd_flechette_speed( "rd_flechette_speed", "2000", FCVAR_CHEAT );
#endif

CASW_Weapon_Flechette2::CASW_Weapon_Flechette2()
{
}


CASW_Weapon_Flechette2::~CASW_Weapon_Flechette2()
{

}

void CASW_Weapon_Flechette2::Precache()
{
	BaseClass::Precache();

#ifdef GAME_DLL
	UTIL_PrecacheOther( "hunter_flechette" );
#endif
}

void CASW_Weapon_Flechette2::PrimaryAttack()
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 )
	{
		Reload();
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();

	if ( pMarine )		// firing from a marine
	{
		m_bIsFiring = true;

		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound( SINGLE );

		if ( m_iClip1 <= AmmoClickPoint() )
		{
			LowAmmoSound();
		}

		// tell the marine to tell its weapon to draw the muzzle flash
		pMarine->DoMuzzleFlash();

		// sets the animation on the weapon model iteself
		SendWeaponAnim( GetPrimaryAttackActivity() );
		pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

		Vector vecDir;
		Vector vecSrc = pMarine->Weapon_ShootPosition();
		if ( pPlayer && pMarine->IsInhabited() )
		{
			// Aim directly at crosshair (so we hit the floor if we miss)
			vecDir = pPlayer->GetCrosshairTracePos() - vecSrc;
			vecDir.NormalizeInPlace();
		}
		else
		{
#ifdef CLIENT_DLL
			Msg( "Error, clientside firing of a weapon that's being controlled by an AI marine\n" );
#else
			vecDir = pMarine->GetActualShootTrajectory( vecSrc );
#endif
		}

		int iShots = 1;

		// Make sure we don't fire more than the amount in the clip
		if ( UsesClipsForAmmo1() )
		{
			iShots = MIN( iShots, m_iClip1 );
			m_iClip1 -= iShots;

#ifdef GAME_DLL
			if ( m_iClip1 <= 0 && pMarine->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
			{
				// check he doesn't have ammo in an ammo bay
				CASW_Weapon_Ammo_Bag *pAmmoBag = NULL;
				CASW_Weapon *pWeapon = pMarine->GetASWWeapon( 0 );
				if ( pWeapon && pWeapon->Classify() == CLASS_ASW_AMMO_BAG )
					pAmmoBag = assert_cast< CASW_Weapon_Ammo_Bag * >( pWeapon );

				if ( !pAmmoBag )
				{
					pWeapon = pMarine->GetASWWeapon( 1 );
					if ( pWeapon && pWeapon->Classify() == CLASS_ASW_AMMO_BAG )
						pAmmoBag = assert_cast< CASW_Weapon_Ammo_Bag * >( pWeapon );
				}
				if ( !pAmmoBag || !pAmmoBag->CanGiveAmmoToWeapon( this ) )
					pMarine->OnWeaponOutOfAmmo( true );
			}
#endif
		}
		else
		{
			iShots = MIN( iShots, pMarine->GetAmmoCount( m_iPrimaryAmmoType ) );
			pMarine->RemoveAmmo( iShots, m_iPrimaryAmmoType );
		}

		// increment shooting stats
#ifndef CLIENT_DLL
		float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine( pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_FLECHETTE_DMG );

		CShotManipulator manipulator( vecDir );
		Vector vecShoot = manipulator.ApplySpread( GetBulletSpread() );

		QAngle vecRocketAngle;
		VectorAngles( vecShoot, vecRocketAngle );
		vecRocketAngle[YAW] += random->RandomFloat( -10, 10 );

		CHunterFlechette *pFlechette = CHunterFlechette::FlechetteCreate( fGrenadeDamage / sk_hunter_dmg_flechette.GetFloat(), vecSrc, vecRocketAngle, GetMarine() );
		if ( pFlechette )
		{
			pFlechette->SetupMarineFlechette( this );
			vecShoot *= rd_flechette_speed.GetFloat();
			pFlechette->Shoot( vecShoot, true );
		}

		if ( pMarine->GetMarineResource() )
		{
			pMarine->GetMarineResource()->UsedWeapon( this, iShots );
			pMarine->OnWeaponFired( this, iShots );
		}

		if ( ASWGameRules() )
			ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;
#endif
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + GetFireRate();
	}
}

void CASW_Weapon_Flechette2::SecondaryAttack()
{
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;


	// dry fire
	SendWeaponAnim( ACT_VM_DRYFIRE );
	BaseClass::WeaponSound( EMPTY );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
}
