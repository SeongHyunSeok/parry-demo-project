#pragma once

#include "../d3dUtil.h"
#include "../component.h"

struct SkyComponent : public Component
{
    SkyComponent(Handle<GameObject> owner) : Component{ owner } {};
    SkyComponent(const SkyComponent& rhs) = delete;
    SkyComponent(SkyComponent&&) = default;

    UINT mSkyTexHeapIndex = 0;
};
