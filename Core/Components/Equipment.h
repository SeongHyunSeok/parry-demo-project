#pragma once

#include "../d3dUtil.h"
#include "../component.h"

struct EquipmentComponent : public Component
{
    EquipmentComponent(Handle<GameObject> owner) : Component{ owner } {};
    EquipmentComponent(const EquipmentComponent& rhs) = delete;
    EquipmentComponent(EquipmentComponent&&) = default;

    EquipmentTypes equipmentType;
};

struct EquipmentOwnerComponent : public Component
{
    EquipmentOwnerComponent(Handle<GameObject> owner, UINT& equippedCBIndexCounter) 
        : Component{ owner } 
    {
        for (int i = 0; i < (UINT)EquipmentTypes::Count; ++i) {
            equippedCBIndices[i] = equippedCBIndexCounter++;
        }
    };
    EquipmentOwnerComponent(const EquipmentOwnerComponent& rhs) = delete;
    EquipmentOwnerComponent(EquipmentOwnerComponent&&) = default;

    UINT equippedCBIndices[(UINT)EquipmentTypes::Count];
};
