#pragma once

namespace gta::Natives
{
    extern std::string_view GetNameByHash(uint64_t hash);
    extern uint64_t GetHashByHandler(uintptr_t handler);
    extern std::unordered_map<uint64_t, uintptr_t> GetAll();
}