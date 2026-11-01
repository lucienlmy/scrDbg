#include "Function.hpp"

namespace scrDbgApp::Decomp
{
    Function::Function(const Disassembler::FunctionInfo& info, uint32_t end, uint32_t reservedSlots)
        : m_Info(info),
          m_End(end),
          m_ReservedSlots(reservedSlots)
    {
    }

    std::string Function::GetFrameVarName(uint32_t index) const
    {
        if (index < m_Info.ArgCount)
            return "Arg_" + std::to_string(index);

        if (index < m_Info.ArgCount + m_ReservedSlots)
            return "FrameSlot_" + std::to_string(index);

        return "Local_" + std::to_string(index - m_Info.ArgCount - m_ReservedSlots);
    }

    int32_t Function::GetInstructionIndexAtOffset(uint32_t offset) const
    {
        auto it = std::lower_bound(m_Instructions.begin(), m_Instructions.end(), offset, [](const Instruction& ins, uint32_t value) {
            return ins.Offset < value;
        });

        if (it == m_Instructions.end() || it->Offset != offset)
            return -1;

        return static_cast<int32_t>(it - m_Instructions.begin());
    }

    void Function::SetInstructions(std::vector<Instruction> instructions)
    {
        m_Instructions = std::move(instructions);

        for (auto& ins : m_Instructions)
        {
            if (ins.Op == IrOp::ENTER)
                ins.Nop();

            if (ins.Op == IrOp::J && (ins.Target <= GetStart() || ins.Target >= GetEnd()))
                ins.Nop();
        }

        CollapseShortCircuits();
    }

    // The compiler lowers short-circuit operators as
    // a && b: <a> DUP JZ L <b> IAND L
    // a || b: <a> DUP INOT JZ L <b> IOR L
    void Function::CollapseShortCircuits()
    {
        const size_t count = m_Instructions.size();

        for (size_t i = 0; i < count; i++)
        {
            if (m_Instructions[i].Op != IrOp::DUP)
                continue;

            // NOPs can sit anywhere in the sequence, the compiler pads instructions that would straddle a code page boundary
            size_t j = i + 1;
            while (j < count && (m_Instructions[j].Op == IrOp::NOP || m_Instructions[j].Op == IrOp::INOT))
                j++;

            if (j >= count || (m_Instructions[j].Op != IrOp::JZ && m_Instructions[j].Op != IrOp::JNZ))
                continue;

            for (size_t k = i; k <= j; k++)
                m_Instructions[k].Nop();

            i = j;
        }
    }
}