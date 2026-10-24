// Written by tools/gen_procreg.py: every process class, for kernel.dat (see procsave.h).

#include "u8port.h"

#include <new>
#include <typeinfo>
#include "amusic.h"
#include "anim.h"
#include "animcall.h"
#include "animclas.h"
#include "animprim.h"
#include "auxfuncs.h"
#include "balls.h"
#include "bark.h"
#include "barkfidg.h"
#include "biostime.h"
#include "camera.h"
#include "combatbr.h"
#include "dsfxman.h"
#include "egg.h"
#include "flicplay.h"
#include "gametime.h"
#include "grantpea.h"
#include "gravity.h"
#include "irgump.h"
#include "item.h"
#include "itemcach.h"
#include "ktimer.h"
#include "loiter.h"
#include "npc.h"
#include "pagan.h"
#include "pal.h"
#include "pathfind.h"
#include "preamble.h"
#include "procmon.h"
#include "profiler.h"
#include "refstore.h"
#include "sprite.h"
#include "statrest.h"
#include "systimer.h"
#include "u8dispat.h"
#include "u8stdint.h"
#include "uprocess.h"
#include "wait.h"
#include "world.h"
#include "procsave.h"

#define PROCESS_CLASS(X) { #X, &typeid(X), (uint16_t)sizeof(X), \
	[](void *to, const void *from) { ::new (to) X(*static_cast<const X *>(from)); } }

const ProcessClass processClasses[] =
{
	PROCESS_CLASS(AccurateTimer),
	PROCESS_CLASS(Advance),
	PROCESS_CLASS(AilTimer),
	PROCESS_CLASS(AltitudeFall),
	PROCESS_CLASS(AnimPrimitive),
	PROCESS_CLASS(Animation),
	PROCESS_CLASS(AvatarDeathWatch),
	PROCESS_CLASS(AvatarSwingToggler),
	PROCESS_CLASS(Ball),
	PROCESS_CLASS(BarkFidget),
	PROCESS_CLASS(BiosTimer),
	PROCESS_CLASS(BlockEnd),
	PROCESS_CLASS(BlockStart),
	PROCESS_CLASS(BogusProcess),
	PROCESS_CLASS(Bow),
	PROCESS_CLASS(Burn),
	PROCESS_CLASS(CameraProcess),
	PROCESS_CLASS(CarefulStep),
	PROCESS_CLASS(Cast1),
	PROCESS_CLASS(Cast2),
	PROCESS_CLASS(Cast3),
	PROCESS_CLASS(ClearFeignDeath),
	PROCESS_CLASS(Climb),
	PROCESS_CLASS(CombatBrain),
	PROCESS_CLASS(CombatStand),
	PROCESS_CLASS(CoolKeyboard),
	PROCESS_CLASS(DeathGumpProcess),
	PROCESS_CLASS(DeathLoader),
	PROCESS_CLASS(DorkProcess),
	PROCESS_CLASS(DrawWeapon),
	PROCESS_CLASS(Drown),
	PROCESS_CLASS(EdgeBalance),
	PROCESS_CLASS(EggHatcher),
	PROCESS_CLASS(Enliven),
	PROCESS_CLASS(EnterCast),
	PROCESS_CLASS(ExitCast),
	PROCESS_CLASS(Explode),
	PROCESS_CLASS(FadeProcess),
	PROCESS_CLASS(FailWait),
	PROCESS_CLASS(FallDown),
	PROCESS_CLASS(FallRecDmg),
	PROCESS_CLASS(FallRecHurt),
	PROCESS_CLASS(FallRecovery),
	PROCESS_CLASS(Fidget),
	PROCESS_CLASS(Fidget2),
	PROCESS_CLASS(FlicPlayer),
	PROCESS_CLASS(FutureAnim),
	PROCESS_CLASS(GameTime),
	PROCESS_CLASS(GetHit),
	PROCESS_CLASS(GetUp),
	PROCESS_CLASS(GiveItem),
	PROCESS_CLASS(GrantPeaceSpell),
	PROCESS_CLASS(GravityTracker),
	PROCESS_CLASS(GravityTrackerCling),
	PROCESS_CLASS(GumpNotifier),
	PROCESS_CLASS(HeadShaker),
	PROCESS_CLASS(HighReach),
	PROCESS_CLASS(ItemCreator),
	PROCESS_CLASS(ItemDestroyer),
	PROCESS_CLASS(Kick),
	PROCESS_CLASS(Kneel),
	PROCESS_CLASS(KneelDown),
	PROCESS_CLASS(KneelUp),
	PROCESS_CLASS(LedgeClimb),
	PROCESS_CLASS(LedgeSwing),
	PROCESS_CLASS(Loiter),
	PROCESS_CLASS(LookLeft),
	PROCESS_CLASS(LookRight),
	PROCESS_CLASS(LowReach),
	PROCESS_CLASS(MainMenuProcess),
	PROCESS_CLASS(MakeSemi),
	PROCESS_CLASS(MidReach),
	PROCESS_CLASS(MusicProcess),
	PROCESS_CLASS(NinjaFlip),
	PROCESS_CLASS(PaganProcess),
	PROCESS_CLASS(PathFinder),
	PROCESS_CLASS(PreambleProcess),
	PROCESS_CLASS(ProcessMonitor),
	PROCESS_CLASS(Profiler),
	PROCESS_CLASS(QuickWait),
	PROCESS_CLASS(RefreshWorld),
	PROCESS_CLASS(ReleaseTrigger),
	PROCESS_CLASS(RestoreMusicProcess),
	PROCESS_CLASS(ResultProcess),
	PROCESS_CLASS(Retreat),
	PROCESS_CLASS(RoundTimer),
	PROCESS_CLASS(Run),
	PROCESS_CLASS(RunningLeap),
	PROCESS_CLASS(Scheduler),
	PROCESS_CLASS(ScrollProcess),
	PROCESS_CLASS(ScrollProcessPlusPlus),
	PROCESS_CLASS(SheathWeapon),
	PROCESS_CLASS(SitDown),
	PROCESS_CLASS(SitUp),
	PROCESS_CLASS(SoundTimer),
	PROCESS_CLASS(SoundTimerIITheSequel),
	PROCESS_CLASS(SpeakerManager),
	PROCESS_CLASS(SpeechProcess),
	PROCESS_CLASS(Sprite),
	PROCESS_CLASS(Stand),
	PROCESS_CLASS(StandingLeap),
	PROCESS_CLASS(StatRestorer),
	PROCESS_CLASS(StdIntHandler),
	PROCESS_CLASS(StorageData),
	PROCESS_CLASS(SuspendAvatar),
	PROCESS_CLASS(SwingWeapon),
	PROCESS_CLASS(SwingWeapon2),
	PROCESS_CLASS(SystemTimer),
	PROCESS_CLASS(Talk),
	PROCESS_CLASS(TeleporterProcess),
	PROCESS_CLASS(Timer),
	PROCESS_CLASS(TypeChanger),
	PROCESS_CLASS(U8Dispatcher),
	PROCESS_CLASS(UnkProcess),
	PROCESS_CLASS(UnstunNpc),
	PROCESS_CLASS(VertLeap),
	PROCESS_CLASS(Wait),
	PROCESS_CLASS(WaitForVictim),
	PROCESS_CLASS(Walk),
	PROCESS_CLASS(Work),
};

const uint16_t numProcessClasses = sizeof(processClasses) / sizeof(processClasses[0]);
