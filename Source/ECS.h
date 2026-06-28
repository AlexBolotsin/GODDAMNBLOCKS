#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

using EntityID = uint32_t;
static constexpr EntityID kNullEntity = 0;

// Sparse-set component pool:
//   - O(1) Has / Get / Add / Remove via hash-map index into dense arrays
//   - Dense m_data / m_ids for cache-friendly iteration
//   - Add a new component type by adding a pool member to World
template<typename T>
class ComponentPool
{
public:
    bool Has(EntityID id) const { return m_index.count(id) != 0; }

    T* Get(EntityID id)
    {
        auto it = m_index.find(id);
        return (it != m_index.end()) ? &m_data[it->second] : nullptr;
    }
    const T* Get(EntityID id) const
    {
        auto it = m_index.find(id);
        return (it != m_index.end()) ? &m_data[it->second] : nullptr;
    }

    T& Add(EntityID id, T comp = {})
    {
        auto it = m_index.find(id);
        if (it != m_index.end())
            return m_data[it->second]; // already present — return existing

        m_index[id] = static_cast<uint32_t>(m_data.size());
        m_ids.push_back(id);
        m_data.push_back(std::move(comp));
        return m_data.back();
    }

    void Remove(EntityID id)
    {
        auto it = m_index.find(id);
        if (it == m_index.end()) return;

        const uint32_t idx  = it->second;
        const uint32_t last = static_cast<uint32_t>(m_data.size()) - 1;

        if (idx != last)
        {
            // Swap with last to keep arrays dense, then fix the moved entry's index
            m_data[idx]         = std::move(m_data[last]);
            m_ids[idx]          = m_ids[last];
            m_index[m_ids[idx]] = idx;
        }
        m_data.pop_back();
        m_ids.pop_back();
        m_index.erase(id);
    }

    void Clear()
    {
        m_index.clear();
        m_ids.clear();
        m_data.clear();
    }

    // Index-based access for iteration (safe even if pools are modified after the loop)
    size_t   Size()             const { return m_data.size(); }
    T&       DataAt(size_t i)         { return m_data[i]; }
    const T& DataAt(size_t i)   const { return m_data[i]; }
    EntityID IdAt(size_t i)     const { return m_ids[i]; }

    // Callback-based iteration — func(EntityID, T&)
    // Snapshot size first: iteration is safe when components are added during the loop
    template<typename Func>
    void ForEach(Func&& func)
    {
        const size_t n = m_data.size();
        for (size_t i = 0; i < n; ++i)
            func(m_ids[i], m_data[i]);
    }

private:
    std::unordered_map<EntityID, uint32_t> m_index;
    std::vector<EntityID>                  m_ids;
    std::vector<T>                         m_data;
};
