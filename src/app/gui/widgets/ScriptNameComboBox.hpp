#pragma once
#include <QComboBox>

namespace scrDbgApp
{
    class ScriptNameComboBox : public QComboBox
    {
        Q_OBJECT

    public:
        explicit ScriptNameComboBox(QWidget* parent = nullptr);

        std::string GetScriptName() const;
        uint32_t GetScriptHash() const;

        void RefreshScripts();

    protected:
        void showPopup() override;
        void focusInEvent(QFocusEvent* event) override;

    private:
        // cached so the statics tab's update timer doesn't walk the thread list every tick
        mutable QString m_ResolvedText;
        mutable std::string m_ResolvedName;
    };
}