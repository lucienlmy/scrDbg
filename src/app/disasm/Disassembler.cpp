#include "Disassembler.hpp"

namespace scrDbgApp
{
    Disassembler::Disassembler(std::unique_ptr<ScriptProgram> program)
        : m_Program(std::move(program)),
          m_Hash(0)
    {
    }

    void Disassembler::Refresh()
    {
        m_Code.clear();
        m_Instructions.clear();
        m_Functions.clear();

        m_Code = m_Program->GetCode();
        m_Hash = m_Program->GetNameHash();

        uint32_t pc = 0;
        while (pc < m_Code.size())
        {
            BuildFunction(pc);

            m_Instructions.push_back(pc);

            pc += GetInstructionSize(pc);
        }
    }

    uint32_t Disassembler::GetHash() const
    {
        return m_Hash;
    }

    const std::vector<uint8_t>& Disassembler::GetCode() const
    {
        return m_Code;
    }

    const ScriptProgram* Disassembler::GetProgram() const
    {
        return m_Program.get();
    }

    int Disassembler::GetFunctionCount() const
    {
        return static_cast<int>(m_Functions.size());
    }

    Disassembler::FunctionInfo Disassembler::GetFunction(int index) const
    {
        if (index < 0 || static_cast<size_t>(index) >= m_Functions.size())
            return {};

        return m_Functions[index];
    }

    uint32_t Disassembler::ResolveFunctionEntry(uint32_t pc) const
    {
        const uint16_t nop = GetOpcodeNumber(Opcodes::NOP);
        const uint16_t jmp = GetOpcodeNumber(Opcodes::J);
        const uint16_t enter = GetOpcodeNumber(Opcodes::ENTER);

        uint32_t cur = pc;
        for (int i = 0; i < 16 && cur < m_Code.size(); i++)
        {
            uint8_t op = GetU8(cur);

            if (op == enter)
                return cur;

            if (op == nop)
            {
                cur += GetInstructionSize(cur);
            }
            else if (op == jmp)
            {
                // Only a jump over filler is padding. A jump over code can be the end of an if/else whose else runs to the end of the function, landing on the next ENTER
                uint32_t next = cur + GetInstructionSize(cur);
                uint32_t target = GetJumpTarget(cur);
                if (target < next)
                    break;

                for (uint32_t filler = next; filler < target; filler++)
                {
                    if (GetU8(filler) != nop)
                        return pc;
                }

                cur = target;
            }
            else
            {
                break;
            }
        }

        return pc;
    }

    bool Disassembler::IsFunctionBoundary(uint32_t pc) const
    {
        if (pc >= m_Code.size())
            return true;

        return GetU8(ResolveFunctionEntry(pc)) == GetOpcodeNumber(Opcodes::ENTER);
    }

    std::optional<Disassembler::FunctionInfo> Disassembler::GetFunctionForPc(uint32_t pc) const
    {
        auto it = std::upper_bound(m_Functions.begin(), m_Functions.end(), pc, [](uint32_t value, const FunctionInfo& func) {
            return value < func.Start;
        });

        if (it == m_Functions.begin())
            return std::nullopt;

        --it;

        if (pc <= it->End)
            return *it;

        return std::nullopt;
    }

    std::vector<uint32_t> Disassembler::GetFunctionInstructions(const FunctionInfo& func) const
    {
        std::vector<uint32_t> pcs;
        for (uint32_t pc = func.Start; pc <= func.End && pc < m_Code.size();)
        {
            pcs.push_back(pc);

            int size = GetInstructionSize(pc);
            if (size <= 0)
                break;
            pc += size;
        }

        return pcs;
    }

    int Disassembler::GetInstructionCount() const
    {
        return static_cast<int>(m_Instructions.size());
    }

    uint32_t Disassembler::GetInstruction(int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_Instructions.size()))
            return 0;

        return m_Instructions[index];
    }

    Disassembler::DecodedInstruction Disassembler::DecodeInstruction(int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_Instructions.size()))
            return {};

        DecodedInstruction result{};

        uint32_t pc = m_Instructions[index];
        int size = GetInstructionSize(pc);

        result.Address.resize(10);
        std::snprintf(result.Address.data(), result.Address.size() + 1, "0x%08X", pc);

        char byte[4];
        result.Bytes.reserve(size * 3);
        for (int i = 0; i < size; i++)
        {
            std::snprintf(byte, sizeof(byte), "%02X ", GetU8(pc + i));
            result.Bytes += byte;
        }

        result.Instruction = DecodeInstructionInternal(index);

        return result;
    }

    std::vector<uint32_t> Disassembler::ScanPattern(const std::vector<std::optional<uint8_t>>& pattern) const
    {
        std::vector<uint32_t> results;

        int codeSize = static_cast<int>(m_Code.size());
        int patSize = static_cast<int>(pattern.size());
        if (patSize == 0 || codeSize < patSize)
            return results;

        for (int i = 0; i + patSize <= codeSize; i++)
        {
            bool match = true;
            for (int j = 0; j < patSize; j++)
            {
                if (pattern[j] && *pattern[j] != m_Code[i + j])
                {
                    match = false;
                    break;
                }
            }

            if (match)
                results.push_back(i);
        }

        return results;
    }
}