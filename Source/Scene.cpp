#include "Scene.h"

Entity& Scene::CreateEntity()
{
    m_entities.push_back(std::make_unique<Entity>());
    return *m_entities.back();
}

void Scene::Clear()
{
    m_entities.clear();
}
