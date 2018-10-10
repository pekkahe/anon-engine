#pragma once

#include <component/Database.hpp>
#include <component/Query.hpp>
#include <core/Defines.hpp>

// Generate function definition for a component assignment function.
#define ADD_COMPONENT_FUNCTION(ComponentName, TableField) \
        void add##ComponentName##(EntityId id, ##ComponentName##&& component) \
        { \
            ##TableField##.assign(id, std::forward<##ComponentName##>(component)); \
        }

namespace eng
{
    class Scene;
    class ISystem : public trait::non_copyable
    {
    public:
        // Notification on the system's registration to a scene.
        virtual void onRegistered(const Scene& scene) = 0;

        // System logic update executed once per frame.
        virtual void update(const Scene& scene) = 0;

        // Push the system's Updated tags into the database.
        virtual void commitUpdated(Database& db) = 0;
        // Push the system's Deleted tags into the database.
        virtual void commitDeleted(Database& db) = 0;
    };

    class System : public ISystem
    {
    public:
        void onRegistered(const Scene& scene) override;

        void commitUpdated(Database& database) override;
        void commitDeleted(Database& database) override;

    protected:
        virtual ~System() {}

        template <typename Component>
        TableRef<Component> createTable(Database& db);
        
        // Build a query with pre-built read-only access to the scene database.
        Query<> query() const { return Query<>(*m_database); }

        // Mark an entity with the Updated tag, for one whole frame,
        // starting from the next frame.
        void markUpdated(EntityId id);
        // Mark an entity with the Deleted tag, for one whole frame,
        // starting from the next frame, after which the entity is
        // removed from the scene database.
        void markDeleted(EntityId id);

    private:
        const Database* m_database;

        // Each system has their own tag component tables, which are synchronized
        // with the scene database at the start of each frame. This ensures that 
        // all systems can react to tags regardless of their update order, and 
        // provides a thread-safety mechanism for concurrent system updated (TBD).

        Table<Updated> m_updated;
        Table<Deleted> m_deleted;
    };

    template<typename Component>
    inline TableRef<Component> System::createTable(Database& db)
    {
        // todo: unnecessary at the moment, but can be an entry point for the system's
        // table registration, i.e. a way to get rid of the ADD_COMPONENT_FUNCTION macro,
        // maybe with some template metaprogramming..?

        return db.createTable<Component>();
    }

    //template<int N, typename... Ts> 
    //using NthTypeOf = typename std::tuple_element<N, std::tuple<Ts...>>::type;

    //template<typename... Ts>
    //using ThirdType = NthTypeOf<2, Ts...>;
    //
    //template <int N, typename... T>
    //struct tuple_element;

    //template <typename T0, typename... T>
    //struct tuple_element<0, T0, T...> 
    //{
    //    typedef T0 type;
    //};

    //template <int N, typename T0, typename... T>
    //struct tuple_element<N, T0, T...>
    //{
    //    typedef typename tuple_element<N - 1, T...>::type type;
    //};
    ////

    //template <typename... Tables>
    //class TableCollection : public trait::non_copyable
    //{
    //public:
    //    TableCollection(Tables... tables) : m_tables(tables...) {}
    //    TableCollection(TableCollection&&) = default;
    //    TableCollection& operator=(TableCollection&&) = default;

    //    template <typename Table>
    //    auto add(Table table)
    //    {
    //        auto c = col(table, std::index_sequence_for<Tables...>());

    //        c.m_indices(sizeof(Tables...), m_indices);

    //        NthTypeOf<sizeof(Tables...), Tables...>
    //    }

    //private:
    //    template <typename T, size_t... Is>
    //    auto col(T&& table, std::index_sequence<Is...>)
    //    {
    //        return TableCollection<Tables..., decltype(table)>(
    //            std::get<Is>(m_tables)..., table);
    //    }

    //private:
    //    std::tuple<Tables...> m_tables;
    //    std::tuple<int...>    m_indices;
    //};
}