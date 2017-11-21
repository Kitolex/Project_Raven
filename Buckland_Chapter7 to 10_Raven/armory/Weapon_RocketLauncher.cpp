#include "Weapon_RocketLauncher.h"
#include "../Raven_Bot.h"
#include "misc/Cgdi.h"
#include "../Raven_Game.h"
#include "../Raven_Map.h"
#include "../lua/Raven_Scriptor.h"
#include "fuzzy/FuzzyOperators.h"


//--------------------------- ctor --------------------------------------------
//-----------------------------------------------------------------------------
RocketLauncher::RocketLauncher(Raven_Bot*   owner):

                      Raven_Weapon(type_rocket_launcher,
                                   script->GetInt("RocketLauncher_DefaultRounds"),
                                   script->GetInt("RocketLauncher_MaxRoundsCarried"),
                                   script->GetDouble("RocketLauncher_FiringFreq"),
                                   script->GetDouble("RocketLauncher_IdealRange"),
                                   script->GetDouble("Rocket_MaxSpeed"),
                                   owner)
{
    //setup the vertex buffer
  const int NumWeaponVerts = 8;
  const Vector2D weapon[NumWeaponVerts] = {Vector2D(0, -3),
                                           Vector2D(6, -3),
                                           Vector2D(6, -1),
                                           Vector2D(15, -1),
                                           Vector2D(15, 1),
                                           Vector2D(6, 1),
                                           Vector2D(6, 3),
                                           Vector2D(0, 3)
                                           };
  for (int vtx=0; vtx<NumWeaponVerts; ++vtx)
  {
    m_vecWeaponVB.push_back(weapon[vtx]);
  }

  //setup the fuzzy module
  InitializeFuzzyModule();

}


//------------------------------ ShootAt --------------------------------------
//-----------------------------------------------------------------------------
inline void RocketLauncher::ShootAt(Vector2D pos)
{ 
  if (NumRoundsRemaining() > 0 && isReadyForNextShot())
  {
    //fire off a rocket!
    m_pOwner->GetWorld()->AddRocket(m_pOwner, pos);

    m_iNumRoundsLeft--;

    UpdateTimeWeaponIsNextAvailable();

    //add a trigger to the game so that the other bots can hear this shot
    //(provided they are within range)
    m_pOwner->GetWorld()->GetMap()->AddSoundTrigger(m_pOwner, script->GetDouble("RocketLauncher_SoundRange"));
  }
}

//---------------------------- Desirability -----------------------------------
//
//-----------------------------------------------------------------------------
double RocketLauncher::GetDesirability(double DistToTarget)
{
  if (m_iNumRoundsLeft == 0)
  {
    m_dLastDesirabilityScore = 0;
  }
  else
  {
    //fuzzify distance and amount of ammo
    m_FuzzyModule.Fuzzify("DistToTarget", DistToTarget);
    m_FuzzyModule.Fuzzify("AmmoStatus", (double)m_iNumRoundsLeft);

    m_dLastDesirabilityScore = m_FuzzyModule.DeFuzzify("Desirability", FuzzyModule::max_av);
  }

  return m_dLastDesirabilityScore;
}

//-------------------------  InitializeFuzzyModule ----------------------------
//
//  set up some fuzzy variables and rules
//-----------------------------------------------------------------------------
void RocketLauncher::InitializeFuzzyModule()
{
  FuzzyVariable& DistToTarget = m_FuzzyModule.CreateFLV("DistToTarget");

  FzSet& Target_Very_Close = DistToTarget.AddLeftShoulderSet("Target_Very_Close",0,25,150);
  FzSet& Target_Close = DistToTarget.AddTriangularSet("Target_Close",25,150,300);
  FzSet& Target_Medium = DistToTarget.AddTriangularSet("Target_Medium",150,300,500);
  FzSet& Target_Far = DistToTarget.AddTriangularSet("Target_Far", 300, 500, 750);
  FzSet& Target_Very_Far = DistToTarget.AddRightShoulderSet("Target_Very_Far", 500, 750, 1000);

  FuzzyVariable& Desirability = m_FuzzyModule.CreateFLV("Desirability");
  FzSet& Very_Desirable = Desirability.AddRightShoulderSet("Very_Desirable", 80, 90, 100);
  FzSet& Desirable = Desirability.AddTriangularSet("Desirable", 60, 80, 100);
  FzSet& Average = Desirability.AddTriangularSet("Average", 40, 60, 80);
  FzSet& Undesirable = Desirability.AddTriangularSet("Undesirable", 20, 40, 60);
  FzSet& Very_Undesirable = Desirability.AddLeftShoulderSet("Very_Undesirable", 0, 20, 40);

  FuzzyVariable& AmmoStatus = m_FuzzyModule.CreateFLV("AmmoStatus");
  FzSet& Ammo_Very_Loads = AmmoStatus.AddRightShoulderSet("Ammo_Very_Loads", 50, 75, 100);
  FzSet& Ammo_Loads = AmmoStatus.AddTriangularSet("Ammo_Loads", 30, 50, 75);
  FzSet& Ammo_Okay = AmmoStatus.AddTriangularSet("Ammo_Okay", 10, 30, 50);
  FzSet& Ammo_Low = AmmoStatus.AddTriangularSet("Ammo_Low", 0, 10, 30);
  FzSet& Ammo_Very_Low = AmmoStatus.AddTriangularSet("Ammo_Very_Low", 0, 0, 10);

  m_FuzzyModule.AddRule(FzAND(Target_Very_Close, Ammo_Very_Low), Very_Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Close, Ammo_Low), Very_Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Close, Ammo_Okay), Very_Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Close, Ammo_Loads), Very_Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Close, Ammo_Very_Loads), Very_Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Very_Low), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Low), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Okay), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Loads), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Very_Loads), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Very_Loads), Very_Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Loads), Very_Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Okay), Very_Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Low), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Very_Loads), Average);

  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Very_Loads), Very_Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Loads), Very_Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Okay), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Low), Average);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Very_Low), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Very_Far, Ammo_Very_Loads), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Far, Ammo_Loads), Average);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Far, Ammo_Okay), Average);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Far, Ammo_Low), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Very_Far, Ammo_Very_Low), Very_Undesirable);


}


//-------------------------------- Render -------------------------------------
//-----------------------------------------------------------------------------
void RocketLauncher::Render()
{
    m_vecWeaponVBTrans = WorldTransform(m_vecWeaponVB,
                                   m_pOwner->Pos(),
                                   m_pOwner->Facing(),
                                   m_pOwner->Facing().Perp(),
                                   m_pOwner->Scale());

  gdi->RedPen();

  gdi->ClosedShape(m_vecWeaponVBTrans);
}