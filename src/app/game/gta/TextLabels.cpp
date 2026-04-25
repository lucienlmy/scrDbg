#include "TextLabels.hpp"
#include "Pointers.hpp"

namespace gta::TextLabels
{
    struct GXT2Entry
    {
        uint32_t KeyHash;
        uint32_t KeyOffset;
    };

    struct GXT2Header
    {
        uint32_t Magic; // 2TXG
        uint32_t EntryCount;
    };

    static std::string SearchTextLabelSlot(uint32_t hash, uintptr_t slot)
    {
        auto header = Pointer(slot).Get<GXT2Header>();

        std::vector<GXT2Entry> entries(header.EntryCount);
        Pointer(slot + sizeof(GXT2Header)).GetBuffer(entries.data(), header.EntryCount * sizeof(GXT2Entry));

        auto it = std::lower_bound(entries.begin(), entries.end(), hash, [](GXT2Entry& entry, uint32_t keyHash) {
            return entry.KeyHash < keyHash;
        });

        if (it != entries.end() && it->KeyHash == hash)
            return Pointer(slot + it->KeyOffset).GetString(4096);

        return {};
    }

    std::string GetTextLabel(uint32_t hash)
    {
        for (int i = 0; i < 23; i++)
        {
            if (auto slot = scrDbgApp::g_Pointers.TextLabels.GetArray<uintptr_t>(i))
            {
                if (auto label = SearchTextLabelSlot(hash, slot); !label.empty())
                    return label;
            }
        }

        return {};
    }
}