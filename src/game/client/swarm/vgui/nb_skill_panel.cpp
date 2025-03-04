#include "cbase.h"
#include "nb_skill_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "StatsBar.h"
#include "vgui_bitmapbutton.h"
#include "asw_marine_skills.h"
#include "asw_gamerules.h"
#include "c_asw_player.h"
#include "asw_briefing.h"
#include "asw_marine_profile.h"
#include "BriefingTooltip.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Skill_Panel::CNB_Skill_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pSkillLabel = new vgui::Label( this, "SkillLabel", "" );
	m_pSkillNumberLabel = new vgui::Label( this, "SkillNumberLabel", "" );	
	m_pSkillBar = new StatsBar( this, "SkillBar" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pSkillButton = new CBitmapButton( this, "SkillImage", "" );
	m_pSkillButton->AddActionSignalTarget( this );
	m_pSkillButton->SetCommand( "SpendPoint" );
	m_pSkillBar->SetShowMaxOnCounter( true );
	m_pSkillBar->m_flBorder = 1.5f;
	m_bSpendPointsMode = false;
	m_szLastSkillImage[0] = 0;
	m_nProfileIndex = -1;
	m_nSkillType = ASW_MARINE_SKILL_INVALID;
}

CNB_Skill_Panel::~CNB_Skill_Panel()
{
	BriefingTooltip::Free();
}

void CNB_Skill_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_szLastSkillImage[0] = 0;

	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_skill_panel.res" );
}

void CNB_Skill_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Skill_Panel::OnThink()
{
	BaseClass::OnThink();

	if ( !MarineSkills() || !Briefing() || m_nSkillType == ASW_MARINE_SKILL_INVALID )
		return;

	int nMaxSkillPoints = MarineSkills()->GetMaxSkillPoints( m_nSkillType );
	const char *szImageName = MarineSkills()->GetSkillImage( m_nSkillType );
	if ( Q_strcmp( m_szLastSkillImage, szImageName ) )
	{
		Q_snprintf( m_szLastSkillImage, sizeof( m_szLastSkillImage ), "%s", szImageName );
		char buffer[ 256 ];
		Q_snprintf( buffer, sizeof( buffer ), "vgui/%s", szImageName );
		
		color32 white;
		white.r = 255;
		white.g = 255;
		white.b = 255;
		white.a = 255;

		color32 dull;
		dull.r = 192;
		dull.g = 192;
		dull.b = 192;
		dull.a = 255;

		m_pSkillButton->SetImage( CBitmapButton::BUTTON_ENABLED, buffer, white );
		m_pSkillButton->SetImage( CBitmapButton::BUTTON_DISABLED, buffer, dull );
		m_pSkillButton->SetImage( CBitmapButton::BUTTON_PRESSED, buffer, white );

		Q_snprintf( buffer, sizeof( buffer ), "vgui/%s_over", szImageName );
		m_pSkillButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, buffer, white );		
	}
	m_pSkillButton->SetEnabled( m_bSpendPointsMode && CanSpendPoint() );

	m_pSkillLabel->SetText( MarineSkills()->GetSkillName( m_nSkillType ) );

	wchar_t wszPointsBuffer[ 24 ];
	V_snwprintf( wszPointsBuffer, ARRAYSIZE( wszPointsBuffer ), L"%d / %d", m_nSkillPoints, nMaxSkillPoints );
	m_pSkillNumberLabel->SetText( wszPointsBuffer );

	m_pSkillBar->ClearMinMax();
	m_pSkillBar->AddMinMax( 0, nMaxSkillPoints );
	m_pSkillBar->Init( m_nSkillPoints, m_nSkillPoints, 0.1f, true, false );

	if ( IsCursorOver() )
	{
		BriefingTooltip::EnsureParent( GetParent() );

		if ( g_hBriefingTooltip.Get() && IsFullyVisible() &&
			g_hBriefingTooltip.Get()->GetTooltipPanel() != this )
		{	
			int tx, ty, w, h;
			tx = ty = 0;
			LocalToScreen(tx, ty);
			GetSize(w, h);
			tx += w * 0.5f;
			ty -= h * 0.01f;

			g_hBriefingTooltip.Get()->SetTooltip( this, MarineSkills()->GetSkillName( m_nSkillType ), MarineSkills()->GetSkillDescription( m_nSkillType ),
				tx, ty );
		}
	}
}

void CNB_Skill_Panel::SetSkillDetails( int nProfileIndex, int nSkillSlot, int nSkillPoints, int nSkillType )
{
	m_nProfileIndex = nProfileIndex;
	m_nSkillSlot = nSkillSlot;
	m_nSkillPoints = nSkillPoints;
	m_nSkillType = nSkillType == -1 ? MarineProfileList()->GetProfile( nProfileIndex )->GetSkillMapping( nSkillSlot ) : ASW_Skill( nSkillType );
}

// NOTE: This function breaks IBriefing abstraction
bool CNB_Skill_Panel::CanSpendPoint()
{
	if ( !ASWGameRules() || m_nProfileIndex == -1 )
		return false;

	return ASWGameRules()->CanSpendPoint( C_ASW_Player::GetLocalASWPlayer(), m_nProfileIndex, m_nSkillSlot );
}

// NOTE: This function breaks IBriefing abstraction
void CNB_Skill_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "SpendPoint" ) )
	{
		if ( CanSpendPoint() )
		{
			// request spend a point in our skill
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			pPlayer->RosterSpendSkillPoint( m_nProfileIndex, m_nSkillSlot );			
		}
		return;
	}
	BaseClass::OnCommand( command );
}

// =========================================

CNB_Skill_Panel_Spending::CNB_Skill_Panel_Spending( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
}

CNB_Skill_Panel_Spending::~CNB_Skill_Panel_Spending()
{

}

void CNB_Skill_Panel_Spending::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_szLastSkillImage[0] = 0;

	BaseClass::BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/ui/nb_skill_panel_spending.res" );
}