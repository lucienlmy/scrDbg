#pragma once
#include "Ast.hpp"
#include <variant>

namespace scrDbgApp::Decomp
{
    class CodeWriter;
    class Function;
    class Tree;

    using TreePtr = std::shared_ptr<Tree>;
    using StatementItem = std::variant<AstPtr, TreePtr>;

    // Control-flow block tree
    class Tree
    {
    public:
        Tree(Function& function, Tree* parent, int32_t offset)
            : Func(function),
              Offset(offset),
              Parent(parent)
        {
        }

        virtual ~Tree() = default;

        virtual bool IsTreeEnd()
        {
            return false;
        }

        // Writes this block (header, braces, body) at indent tabs
        virtual void Emit(CodeWriter& writer, int indent) const
        {
            EmitBody(writer, indent);
        }

        void EmitBody(CodeWriter& writer, int indent) const;

        bool ReachedAddress(uint32_t address) const; // True once this block's instruction pointer has reached address

        Function& Func;
        std::vector<StatementItem> Statements;
        int32_t Offset;
        Tree* Parent;

        std::vector<uint32_t> HeaderAddresses; // Instructions the block's header line stands for beyond what its condition/value expression renders

    protected:
        void EmitBlock(CodeWriter& writer, int indent, bool skipBraces) const;

    private:
        virtual bool SuppressBlankBeforeLeaf(const AstPtr&) const
        {
            return false;
        }
    };

    class Main : public Tree
    {
    public:
        explicit Main(Function& function)
            : Tree(function, nullptr, 0)
        {
        }

        bool IsTreeEnd() override;
    };

    class If : public Tree
    {
    public:
        If(Function& function, Tree* parent, int32_t offset, AstPtr condition, uint32_t endAddress)
            : Tree(function, parent, offset),
              Condition(std::move(condition)),
              EndAddress(endAddress)
        {
        }

        bool IsTreeEnd() override;
        bool CanSkipBraces() const;
        void Emit(CodeWriter& writer, int indent) const override;

        AstPtr Condition;
        uint32_t EndAddress;
        TreePtr ElseTree;
        std::vector<TreePtr> ElseIfTrees;
    };

    class Else : public Tree
    {
    public:
        Else(Function& function, Tree* parent, int32_t offset, uint32_t endAddress)
            : Tree(function, parent, offset),
              EndAddress(endAddress)
        {
        }

        bool IsTreeEnd() override;
        void Emit(CodeWriter& writer, int indent) const override;

        uint32_t EndAddress;
    };

    class ElseIf : public Tree
    {
    public:
        ElseIf(Function& function, Tree* parent, AstPtr condition)
            : Tree(function, parent, -1),
              Condition(std::move(condition))
        {
        }

        bool IsTreeEnd() override
        {
            return true;
        }

        void Emit(CodeWriter& writer, int indent) const override;

        AstPtr Condition;
    };

    class While : public Tree
    {
    public:
        While(Function& function, Tree* parent, int32_t offset, AstPtr condition, uint32_t breakAddress)
            : Tree(function, parent, offset),
              Condition(std::move(condition)),
              BreakAddress(breakAddress)
        {
        }

        bool IsTreeEnd() override;
        void Emit(CodeWriter& writer, int indent) const override;

        AstPtr Condition;
        uint32_t BreakAddress;

        // for loop support
        static constexpr uint32_t NO_CONTINUE = 0xFFFFFFFF;
        uint32_t ContinueAddress = NO_CONTINUE;
        int32_t StepStart = -1; // index in Statements of the latch's label
        bool CanBeFor = true;   // cleared when something other than a continue needs the latch's label
        std::vector<std::shared_ptr<Continue>> Continues;
        std::vector<AstPtr> Step; // set when it's emitted as a for
        bool IsFor = false;
    };

    // The loop's tail is a backward conditional jump to the first body instruction
    // // the body is decoded until that jump is reached, which supplies the condition..
    class DoWhile : public Tree
    {
    public:
        DoWhile(Function& function, Tree* parent, int32_t offset, int32_t tailIndex, uint32_t breakAddress)
            : Tree(function, parent, offset),
              TailIndex(tailIndex),
              BreakAddress(breakAddress),
              Finished(false)
        {
        }

        bool IsTreeEnd() override
        {
            return Finished;
        }

        void Emit(CodeWriter& writer, int indent) const override;

        AstPtr Condition;
        int32_t TailIndex;     // instruction index of the backward conditional jump
        uint32_t BreakAddress; // address right after the tail jump
        bool Finished;
        std::vector<uint32_t> TailAddresses; // the tail jump, for the "} while (...);" line
    };

    class Switch : public Tree
    {
    public:
        // The Case subtrees are created here.
        Switch(Function& function, Tree* parent, int32_t offset, std::vector<std::pair<uint32_t, std::vector<AstPtr>>> cases, uint32_t breakAddress, AstPtr value);

        bool IsTreeEnd() override
        {
            return true;
        }

        void Emit(CodeWriter& writer, int indent) const override;

        uint32_t BreakAddress;
        AstPtr Value;
    };

    class Case : public Tree
    {
    public:
        Case(Function& function, Tree* parent, int32_t offset, std::vector<AstPtr> labels, uint32_t breakAddress)
            : Tree(function, parent, offset),
              EndIndex(-1),
              BreakAddress(breakAddress),
              Labels(std::move(labels))
        {
        }

        bool IsTreeEnd() override;
        void Emit(CodeWriter& writer, int indent) const override;

        int32_t EndIndex; // instruction index where the next case starts, -1 for the last case
        uint32_t BreakAddress;

    private:
        bool SuppressBlankBeforeLeaf(const AstPtr& statement) const override;

        std::vector<AstPtr> Labels;
    };
}