////////////////////////////////////////////////////////////////////////////////
//
// Project-Hellscream https://hellscream.org
// Copyright (C) 2018-2020 Project-Hellscream-6.2
// Discord https://discord.gg/CWCF3C9
//
////////////////////////////////////////////////////////////////////////////////

#include "VMapFactory.h"
#include "VMapManager2.h"
#include "VMapDefinitions.h"
#include "WorldModel.h"

#include "GameObjectModel.h"
#include "Log.h"
#include "GameObject.h"
#include "Creature.h"
#include "TemporarySummon.h"
#include "Object.h"
#include "DBCStores.h"
#include "World.h"

using G3D::Vector3;
using G3D::Ray;
using G3D::AABox;

//#define SPAWN_CORNERS 1

struct GameobjectModelData
{
    GameobjectModelData(const std::string& name_, const AABox& box) :
        bound(box), name(name_) { }

    AABox bound;
    std::string name;
};

typedef std::unordered_map<uint32, GameobjectModelData> ModelList;
ModelList model_list;

void LoadGameObjectModelList()
{
#ifndef NO_CORE_FUNCS
    uint32 oldMSTime = getMSTime();
#endif

    FILE* model_list_file = fopen((sWorld->GetDataPath() + "vmaps/" + VMAP::GAMEOBJECT_MODELS).c_str(), "rb");
    if (!model_list_file)
    {
        sLog->outError(LOG_FILTER_MAPS, "Unable to open '%s' file.", VMAP::GAMEOBJECT_MODELS);
        return;
    }

    uint32 name_length, displayId;
    char buff[500];
    while (true)
    {
        Vector3 v1, v2;
        if (fread(&displayId, sizeof(uint32), 1, model_list_file) != 1)
            if (feof(model_list_file))  // EOF flag is only set after failed reading attempt
                break;

        if (fread(&name_length, sizeof(uint32), 1, model_list_file) != 1
            || name_length >= sizeof(buff)
            || fread(&buff, sizeof(char), name_length, model_list_file) != name_length
            || fread(&v1, sizeof(Vector3), 1, model_list_file) != 1
            || fread(&v2, sizeof(Vector3), 1, model_list_file) != 1)
        {
            sLog->outDebug(LOG_FILTER_MAPS, "File '%s' seems to be corrupted!", VMAP::GAMEOBJECT_MODELS);
            break;
        }

        if (v1.isNaN() || v2.isNaN())
        {
            sLog->outDebug(LOG_FILTER_MAPS, "File '%s' Model '%s' has invalid v1%s v2%s values!", VMAP::GAMEOBJECT_MODELS, std::string(buff, name_length).c_str(), v1.toString().c_str(), v2.toString().c_str());
            continue;
        }

        model_list.insert
        (
            ModelList::value_type( displayId, GameobjectModelData(std::string(buff, name_length), AABox(v1, v2)) )
        );
    }

    fclose(model_list_file);
    sLog->outDebug(LOG_FILTER_MAPS, ">> Loaded %u GameObject models in %u ms", uint32(model_list.size()), GetMSTimeDiffToNow(oldMSTime));
}

GameObjectModel::~GameObjectModel()
{
    if (iModel)
        ((VMAP::VMapManager2*)VMAP::VMapFactory::createOrGetVMapManager())->releaseModelInstance(name);
}

bool GameObjectModel::initialize(const GameObject& go, const GameObjectDisplayInfoEntry& info)
{
    ModelList::const_iterator it = model_list.find(info.Displayid);
    if (it == model_list.end())
        return false;

    G3D::AABox mdl_box(it->second.bound);
    // ignore models with no bounds
    if (mdl_box == G3D::AABox::zero())
    {
        sLog->outError(LOG_FILTER_MAPS, "GameObject model %s has zero bounds, loading skipped", it->second.name.c_str());
        return false;
    }

    iModel = ((VMAP::VMapManager2*)VMAP::VMapFactory::createOrGetVMapManager())->acquireModelInstance(sWorld->GetDataPath() + "vmaps/", it->second.name);

    if (!iModel)
        return false;

    name = it->second.name;
    //flags = VMAP::MOD_M2;
    //adtId = 0;
    //ID = 0;
    iPos = Vector3(go.GetPositionX(), go.GetPositionY(), go.GetPositionZ());
    phasemask = go.GetPhaseMask();
    iScale = go.GetFloatValue(OBJECT_FIELD_SCALE);
    iInvScale = 1.0f / iScale;

    G3D::Matrix3 iRotation = G3D::Matrix3::fromEulerAnglesZYX(go.GetOrientation(), 0, 0);
    iInvRot = iRotation.inverse();
    // transform bounding box:
    mdl_box = AABox(mdl_box.low() * iScale, mdl_box.high() * iScale);
    AABox rotated_bounds;
    for (int i = 0; i < 8; ++i)
        rotated_bounds.merge(iRotation * mdl_box.corner(i));

    iBound = rotated_bounds + iPos;
#ifdef SPAWN_CORNERS
    // test:
    for (int i = 0; i < 8; ++i)
    {
        Vector3 pos(iBound.corner(i));
        Position l_PosSpawn;
        l_PosSpawn.m_positionX = pos.x;
        l_PosSpawn.m_positionY = pos.y;
        l_PosSpawn.m_positionZ = pos.z;
        go.SummonCreature((uint32)1, l_PosSpawn, (TempSummonType)TEMPSUMMON_MANUAL_DESPAWN);
    }
#endif

    owner = &go;
    return true;
}

GameObjectModel* GameObjectModel::Create(const GameObject& go)
{
    const GameObjectDisplayInfoEntry* info = sGameObjectDisplayInfoStore.LookupEntry(go.GetDisplayId());
    if (!info)
        return NULL;

    GameObjectModel* mdl = new GameObjectModel();
    if (!mdl->initialize(go, *info))
    {
        delete mdl;
        return NULL;
    }

    return mdl;
}

bool GameObjectModel::intersectRay(const G3D::Ray& ray, float& MaxDist, bool StopAtFirstHit, uint32 ph_mask) const
{
    if (!(phasemask & ph_mask) || !owner->isSpawned())
        return false;

    float time = ray.intersectionTime(iBound);
    if (time == G3D::finf())
        return false;

    // child bounds are defined in object space:
    Vector3 p = iInvRot * (ray.origin() - iPos) * iInvScale;
    Ray modRay(p, iInvRot * ray.direction());
    float distance = MaxDist * iInvScale;
    bool hit = iModel->IntersectRay(modRay, distance, StopAtFirstHit);
    if (hit)
    {
        distance *= iScale;
        MaxDist = distance;
    }
    return hit;
}

bool GameObjectModel::Relocate(const GameObject& go)
{
    if (!iModel)
        return false;

    ModelList::const_iterator it = model_list.find(go.GetDisplayId());
    if (it == model_list.end())
        return false;

    G3D::AABox mdl_box(it->second.bound);
    // ignore models with no bounds
    if (mdl_box == G3D::AABox::zero())
    {
        sLog->outError(LOG_FILTER_MAPS, "GameObject model %s has zero bounds, loading skipped", it->second.name.c_str());
        return false;
    }

    iPos = Vector3(go.GetPositionX(), go.GetPositionY(), go.GetPositionZ());

    G3D::Matrix3 iRotation = G3D::Matrix3::fromEulerAnglesZYX(go.GetOrientation(), 0, 0);
    iInvRot = iRotation.inverse();
    // transform bounding box:
    mdl_box = AABox(mdl_box.low() * iScale, mdl_box.high() * iScale);
    AABox rotated_bounds;
    for (int i = 0; i < 8; ++i)
        rotated_bounds.merge(iRotation * mdl_box.corner(i));

    iBound = rotated_bounds + iPos;
#ifdef SPAWN_CORNERS
    // test:
    for (int i = 0; i < 8; ++i)
    {
        Vector3 pos(iBound.corner(i));
        Position l_PosSpawn;
        l_PosSpawn.m_positionX = pos.x;
        l_PosSpawn.m_positionY = pos.y;
        l_PosSpawn.m_positionZ = pos.z;
        go.SummonCreature((uint32)1, l_PosSpawn, (TempSummonType)TEMPSUMMON_MANUAL_DESPAWN);
    }
#endif

    return true;
}
