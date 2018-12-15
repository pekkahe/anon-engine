#include <Precompiled.hpp>
#include <component/System.hpp>

#include <scene/Scene.hpp>

using namespace eng;

void System::onRegistered(const Scene& scene)
{
    m_database = &scene.database();
}

void System::commitUpdated(Database& db)
{
    auto& updated = db.table<Updated>();

    m_updatedTable.forEach([&](EntityId id, Updated&)
    {
        updated.assign(id, Updated());
    });
    m_updatedTable.clear();
}

void System::commitDeleted(Database& db)
{
    auto& deleted = db.table<Deleted>();

    m_deletedTable.forEach([&](EntityId id, Deleted&)
    {
        deleted.assign(id, Deleted());
    });
    m_deletedTable.clear();
}

void System::markUpdated(EntityId id)
{
    m_updatedTable.assign(id, Updated());
}

void System::markDeleted(EntityId id)
{
    m_deletedTable.assign(id, Deleted());
}