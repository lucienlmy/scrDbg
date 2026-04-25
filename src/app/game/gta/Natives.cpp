#include "Natives.hpp"
#include "Pointers.hpp"
#include "ResourceLoader.hpp"

namespace gta::Natives
{
    std::string_view GetNameByHash(uint64_t hash)
    {
        return scrDbgShared::NativesBin::GetNameByHash(hash);
    }

    // TO-DO: Some natives (especially nullsubs) share the same handler.
    // This makes it impossible to determine which native was actually called.
    // Change this function to return all hashes associated with the handler, rather than stopping at the first match. This applies to the code in scrDbgLib as well.
    uint64_t GetHashByHandler(uintptr_t handler)
    {
        if (!scrDbgApp::g_Pointers.NativeRegistrationTable || !handler)
            return 0;

        uint8_t buffer[0xC8]{}; // Enough to read it all I guess (scrNativeRegistration + hashes)
        for (int bucket = 0; bucket < 256; ++bucket)
        {
            uintptr_t registration = scrDbgApp::g_Pointers.NativeRegistrationTable.GetArray<uintptr_t>(bucket);

            while (registration)
            {
                if (!Pointer(registration).GetBuffer(buffer, sizeof(buffer)))
                    break;

                uint32_t numEntries1 = *reinterpret_cast<uint32_t*>(buffer + 0x48);
                uint32_t numEntries2 = *reinterpret_cast<uint32_t*>(buffer + 0x4C);
                uint32_t numEntries = numEntries1 ^ static_cast<uint32_t>(registration + 0x48) ^ numEntries2;

                if (numEntries > 7)
                    numEntries = 7;

                auto handlers = reinterpret_cast<uintptr_t*>(buffer + 0x10);
                for (uint32_t j = 0; j < numEntries; ++j)
                {
                    if (handlers[j] == handler)
                    {
                        uintptr_t addr = registration + 0x54 + 16ULL * j;

                        uint32_t key = static_cast<uint32_t>(addr) ^ Pointer(addr).Add(8).Get<uint32_t>();
                        uint32_t low = key ^ Pointer(addr).Add(0).Get<uint32_t>();
                        uint32_t high = key ^ Pointer(addr).Add(4).Get<uint32_t>();
                        return (static_cast<uint64_t>(high) << 32) | low;
                    }
                }

                uint32_t key = static_cast<uint32_t>(registration) ^ *reinterpret_cast<uint32_t*>(buffer + 8);
                uint32_t low = key ^ *reinterpret_cast<uint32_t*>(buffer + 0);
                uint32_t high = key ^ *reinterpret_cast<uint32_t*>(buffer + 4);
                registration = (static_cast<uint64_t>(high) << 32) | low;
            }
        }

        return 0;
    }

    std::unordered_map<uint64_t, uintptr_t> GetAll()
    {
        std::unordered_map<uint64_t, uintptr_t> result;

        if (!scrDbgApp::g_Pointers.NativeRegistrationTable)
            return result;

        uint8_t buffer[0xC8]{};

        for (int bucket = 0; bucket < 256; ++bucket)
        {
            uintptr_t registration = scrDbgApp::g_Pointers.NativeRegistrationTable.GetArray<uintptr_t>(bucket);

            while (registration)
            {
                if (!Pointer(registration).GetBuffer(buffer, sizeof(buffer)))
                    break;

                uint32_t numEntries1 = *reinterpret_cast<uint32_t*>(buffer + 0x48);
                uint32_t numEntries2 = *reinterpret_cast<uint32_t*>(buffer + 0x4C);
                uint32_t numEntries = numEntries1 ^ static_cast<uint32_t>(registration + 0x48) ^ numEntries2;

                if (numEntries > 7)
                    numEntries = 7;

                auto handlers = reinterpret_cast<uintptr_t*>(buffer + 0x10);
                for (uint32_t j = 0; j < numEntries; ++j)
                {
                    uintptr_t handler = handlers[j];
                    uintptr_t addr = registration + 0x54 + 16ULL * j;

                    uint32_t key = static_cast<uint32_t>(addr) ^ Pointer(addr).Add(8).Get<uint32_t>();
                    uint32_t low = key ^ Pointer(addr).Add(0).Get<uint32_t>();
                    uint32_t high = key ^ Pointer(addr).Add(4).Get<uint32_t>();
                    uint64_t hash = (static_cast<uint64_t>(high) << 32) | low;

                    result[hash] = handler;
                }

                uint32_t key = static_cast<uint32_t>(registration) ^ *reinterpret_cast<uint32_t*>(buffer + 8);
                uint32_t low = key ^ *reinterpret_cast<uint32_t*>(buffer + 0);
                uint32_t high = key ^ *reinterpret_cast<uint32_t*>(buffer + 4);
                registration = (static_cast<uint64_t>(high) << 32) | low;
            }
        }

        return result;
    }
}