////////////////////////////////////////////////////////////////////////////////
//
// Project-Hellscream https://hellscream.org
// Copyright (C) 2018-2020 Project-Hellscream-6.2
// Discord https://discord.gg/CWCF3C9
//
////////////////////////////////////////////////////////////////////////////////

#ifndef SCRIPTEDCREATURE_H_
#define SCRIPTEDCREATURE_H_

#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "InstanceScript.h"
#include "LFGMgr.h"

#define CAST_PLR(a)     (dynamic_cast<Player*>(a))
#define CAST_CRE(a)     (dynamic_cast<Creature*>(a))
#define CAST_AI(a, b)   (dynamic_cast<a*>(b))

class InstanceScript;

class SummonList : public std::list<uint64>
{
    public:
        explicit SummonList(Creature* creature) : me(creature) {}
        void Summon(Creature* summon) { push_back(summon->GetGUID()); }
        void Despawn(Creature* summon) { remove(summon->GetGUID()); }
        void DespawnEntry(uint32 entry);
        void DespawnAll();

        template <class Predicate> void DoAction(int32 info, Predicate& predicate, uint16 max = 0)
        {
            // We need to use a copy of SummonList here, otherwise original SummonList would be modified
            std::list<uint64> listCopy = *this;
            JadeCore::Containers::RandomResizeList<uint64, Predicate>(listCopy, predicate, max);
            for (iterator i = listCopy.begin(); i != listCopy.end(); )
            {
                Creature* summon = Unit::GetCreature(*me, *i++);
                if (summon && summon->IsAIEnabled)
                    summon->AI()->DoAction(info);
            }
        }

        void DoZoneInCombat(uint32 entry = 0);
        void RemoveNotExisting();
        bool HasEntry(uint32 entry);
    private:
        Creature* me;
};

class EntryCheckPredicate
{
    public:
        EntryCheckPredicate(uint32 entry) : _entry(entry) {}
        bool operator()(uint64 guid) { return GUID_ENPART(guid) == _entry; }

    private:
        uint32 _entry;
};

class DummyEntryCheckPredicate
{
    public:
        bool operator()(uint64) { return true; }
};

struct ScriptedAI : public CreatureAI
{
    explicit ScriptedAI(Creature* creature);
    virtual ~ScriptedAI() {}

    // *************
    //CreatureAI Functions
    // *************

    void AttackStartNoMove(Unit* target);

    // Called at any Damage from any attacker (before damage apply)
    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, SpellInfo const* /*p_SpellInfo*/) {}

    //Called at World update tick
    virtual void UpdateAI(uint32 const p_Diff);
    void UpdateOperations(uint32 const p_Diff);

    //Called at creature death
    void JustDied(Unit* /*killer*/) {}

    //Called at creature killing another unit
    void KilledUnit(Unit* /*victim*/) {}

    // Called when the creature summon successfully other creature
    void JustSummoned(Creature* /*summon*/) {}

    // Called when a summoned creature is despawned
    void SummonedCreatureDespawn(Creature* /*summon*/) {}

    // Called when hit by a spell
    void SpellHit(Unit* /*caster*/, SpellInfo const* /*spell*/) {}

    // Called when spell hits a target
    void SpellHitTarget(Unit* /*target*/, SpellInfo const* /*spell*/) {}

    /// Called when spell miss a target
    void SpellMissTarget(Unit* /*p_Target*/, SpellInfo const* /*p_SpellInfo*/, SpellMissInfo /*p_MissInfo*/) { }

    //Called at waypoint reached or PointMovement end
    void MovementInform(uint32 /*type*/, uint32 /*id*/) {}

    // Called when AI is temporarily replaced or put back when possess is applied or removed
    void OnPossess(bool /*apply*/) {}

    // Called at any threat added from any attacker (before threat apply)
    void OnAddThreat(Unit* /*victim*/, float& /*fThreat*/, SpellSchoolMask /*schoolMask*/, SpellInfo const* /*threatSpell*/) {}

    // *************
    // Variables
    // *************

    //Pointer to creature we are manipulating
    Creature* me;

    //For fleeing
    bool IsFleeing;

    SummonList summons;
    EventMap events;

    // *************
    //Pure virtual functions
    // *************

    //Called at creature reset either by death or evade
    void Reset() {}

    //Called at creature aggro either by MoveInLOS or Attack Start
    void EnterCombat(Unit* /*victim*/) {}

    // *************
    //AI Helper Functions
    // *************

    //Start movement toward victim
    void DoStartMovement(Unit* target, float distance = 0.0f, float angle = 0.0f);

    //Start no movement on victim
    void DoStartNoMovement(Unit* target);

    //Stop attack of current victim
    void DoStopAttack();

    //Cast spell by spell info
    void DoCastSpell(Unit* target, SpellInfo const* spellInfo, bool triggered = false);

    //Plays a sound to all nearby players
    void DoPlaySoundToSet(WorldObject* source, uint32 soundId);

    //Drops all threat to 0%. Does not remove players from the threat list
    void DoResetThreat();

    float DoGetThreat(Unit* unit);
    void DoModifyThreatPercent(Unit* unit, int32 pct);

    void DoTeleportTo(float x, float y, float z, uint32 time = 0);
    void DoTeleportTo(float const pos[4]);

    //Teleports a player without dropping threat (only teleports to same map)
    void DoTeleportPlayer(Unit* unit, float x, float y, float z, float o);
    void DoTeleportAll(float x, float y, float z, float o);

    //Returns friendly unit with the most amount of hp missing from max hp
    Unit* DoSelectLowestHpFriendly(float range, uint32 minHPDiff = 1);

    //Returns a list of friendly CC'd units within range
    std::list<Creature*> DoFindFriendlyCC(float range);

    //Returns a list of all friendly units missing a specific buff within range
    std::list<Creature*> DoFindFriendlyMissingBuff(float range, uint32 spellId);

    //Return a player with at least minimumRange from me
    Player* GetPlayerAtMinimumRange(float minRange);

    //Spawns a creature relative to me
    Creature* DoSpawnCreature(uint32 entry, float offsetX, float offsetY, float offsetZ, float angle, uint32 type, uint32 despawntime);

    bool HealthBelowPct(uint32 pct) const { return me->HealthBelowPct(pct); }
    bool HealthAbovePct(uint32 pct) const { return me->HealthAbovePct(pct); }

    //Returns spells that meet the specified criteria from the creatures spell list
    SpellInfo const* SelectSpell(Unit* target, uint32 school, uint32 mechanic, SelectTargetType targets, uint32 powerCostMin, uint32 powerCostMax, float rangeMin, float rangeMax, SelectEffect effect);

    void SetEquipmentSlots(bool loadDefault, int32 mainHand = EQUIP_NO_CHANGE, int32 offHand = EQUIP_NO_CHANGE, int32 ranged = EQUIP_NO_CHANGE);

    //Generally used to control if MoveChase() is to be used or not in AttackStart(). Some creatures does not chase victims
    void SetCombatMovement(bool allowMovement);
    bool IsCombatMovementAllowed() const { return _isCombatMovementAllowed; }

    bool EnterEvadeIfOutOfCombatArea(uint32 const diff);

    // return the dungeon or raid difficulty
    Difficulty GetDifficulty() const { return _difficulty; }

    // return true for 25 man or 25 man heroic mode
    bool Is25ManRaid() const { return _difficulty == Difficulty::Difficulty25N || _difficulty == Difficulty::Difficulty25HC || IsLFR(); }
    bool IsLFR() const { return _difficulty == Difficulty::DifficultyRaidTool || _difficulty == Difficulty::DifficultyRaidLFR; }
    bool IsHeroic() const { return me->GetMap()->IsHeroic(); }
    bool IsMythic() const { return me->GetMap()->IsMythic(); }

    template<class T> inline
    const T& DUNGEON_MODE(const T& normal5, const T& heroic10) const
    {
        switch (_difficulty)
        {
            case DifficultyNormal:
                return normal5;
            case DifficultyHeroic:
                return heroic10;
            default:
                break;
        }

        return heroic10;
    }

    template<class T> inline
    const T& RAID_MODE(const T& normal10, const T& normal25) const
    {
        switch (_difficulty)
        {
            case Difficulty10N:
                return normal10;
            case Difficulty25N:
                return normal25;
            default:
                break;
        }

        return normal25;
    }

    template<class T> inline
    const T& RAID_MODE(const T& normal10, const T& normal25, const T& heroic10, const T& heroic25) const
    {
        switch (_difficulty)
        {
            case Difficulty10N:
                return normal10;
            case Difficulty25N:
                return normal25;
            case Difficulty10HC:
                return heroic10;
            case Difficulty25HC:
                return heroic25;
            default:
                break;
        }

        return heroic25;
    }

    /// Add timed delayed operation
    /// @p_Timeout  : Delay time
    /// @p_Function : Callback function
    void AddTimedDelayedOperation(uint32 p_Timeout, std::function<void()> && p_Function)
    {
        m_EmptyWarned = false;
        m_TimedDelayedOperations.push_back(std::pair<uint32, std::function<void()>>(p_Timeout, p_Function));
    }

    /// Called after last delayed operation was deleted
    /// Do whatever you want
    virtual void LastOperationCalled() { }

    void ClearDelayedOperations()
    {
        m_TimedDelayedOperations.clear();
        m_EmptyWarned = false;
    }

    std::vector<std::pair<int32, std::function<void()>>>    m_TimedDelayedOperations;   ///< Delayed operations
    bool                                                    m_EmptyWarned;              ///< Warning when there are no more delayed operations

    private:
        Difficulty _difficulty;
        uint32 _evadeCheckCooldown;
        bool _isCombatMovementAllowed;
};

struct Scripted_NoMovementAI : public ScriptedAI
{
    Scripted_NoMovementAI(Creature* creature) : ScriptedAI(creature) {}
    virtual ~Scripted_NoMovementAI() {}

    //Called at each attack of me by any victim
    void AttackStart(Unit* target);
};

class BossAI : public ScriptedAI
{
    public:
        BossAI(Creature* creature, uint32 bossId);
        virtual ~BossAI() {}

        InstanceScript* const instance;
        BossBoundaryMap const* GetBoundary() const { return _boundary; }

        void JustSummoned(Creature* summon);
        void SummonedCreatureDespawn(Creature* summon);

        virtual void UpdateAI(uint32 const p_Diff);

        // Hook used to execute events scheduled into EventMap without the need
        // to override UpdateAI
        // note: You must re-schedule the event within this method if the event
        // is supposed to run more than once
        virtual void ExecuteEvent(uint32 const /*eventId*/) { }

        void Reset() { _Reset(); }
        void EnterCombat(Unit* /*who*/) { _EnterCombat(); }
        void JustDied(Unit* /*killer*/) { _JustDied(); }
        void JustReachedHome() { _JustReachedHome(); }

    protected:
        void _Reset();
        void _EnterCombat();
        void _JustDied();
        void _JustReachedHome() { me->setActive(false); }

        bool CheckInRoom()
        {
            if (CheckBoundary(me))
                return true;

            EnterEvadeMode();
            return false;
        }

        bool CheckBoundary(Unit* who);
        void TeleportCheaters();

        EventMap events;
        SummonList summons;

    private:
        BossBoundaryMap const* const _boundary;
        uint32 const _bossId;
};

class WorldBossAI : public ScriptedAI
{
    public:
        WorldBossAI(Creature* creature);
        virtual ~WorldBossAI() {}

        void JustSummoned(Creature* summon);
        void SummonedCreatureDespawn(Creature* summon);

        virtual void UpdateAI(uint32 const p_Diff);

        // Hook used to execute events scheduled into EventMap without the need
        // to override UpdateAI
        // note: You must re-schedule the event within this method if the event
        // is supposed to run more than once
        virtual void ExecuteEvent(uint32 const /*eventId*/) { }

        void Reset() { _Reset(); }
        void EnterCombat(Unit* /*who*/) { _EnterCombat(); }
        void JustDied(Unit* /*killer*/) { _JustDied(); }

    protected:
        void _Reset();
        void _EnterCombat();
        void _JustDied();

        EventMap events;
        SummonList summons;
};

// SD2 grid searchers.
Creature* GetClosestCreatureWithEntry(WorldObject* source, uint32 entry, float maxSearchRange, bool alive = true);
GameObject* GetClosestGameObjectWithEntry(WorldObject* source, uint32 entry, float maxSearchRange);
void GetCreatureListWithEntryInGrid(std::list<Creature*>& list, WorldObject* source, uint32 entry, float maxSearchRange);
void GetGameObjectListWithEntryInGrid(std::list<GameObject*>& list, WorldObject* source, uint32 entry, float maxSearchRange);
void GetPlayerListInGrid(std::list<Player*>& list, WorldObject* source, float maxSearchRange);

void GetPositionWithDistInOrientation(Position* pUnit, float dist, float orientation, float& x, float& y);
void GetPositionWithDistInOrientation(Position* fromPos, float dist, float orientation, Position& movePosition);

void GetRandPosFromCenterInDist(float centerX, float centerY, float dist, float& x, float& y);
void GetRandPosFromCenterInDist(Position* centerPos, float dist, Position& movePosition);

void GetPositionWithDistInFront(Position* centerPos, float dist, Position& movePosition);

#endif // SCRIPTEDCREATURE_H_
