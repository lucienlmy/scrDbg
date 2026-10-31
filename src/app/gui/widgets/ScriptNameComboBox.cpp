#include "ScriptNameComboBox.hpp"
#include <QCompleter>
#include <QLineEdit>

namespace scrDbgApp
{
    static std::vector<std::pair<std::string, std::string>> GetRunningScripts()
    {
        bool fullNames = g_Game->GetType() == GameType::RDR2;

        std::vector<std::pair<std::string, std::string>> scripts;
        for (const auto& thread : g_Game->GetThreads())
        {
            if (thread->GetState() == ScriptThread::KILLED || thread->GetStackSize() == 0)
                continue;

            std::string name = thread->GetScriptName();
            std::string hashedName = name;
            if (fullNames)
            {
                if (auto program = g_Game->GetProgram(thread->GetScriptHash()))
                    hashedName = program->GetName();
            }

            if (!hashedName.empty())
                scripts.emplace_back(std::move(name), std::move(hashedName));
        }

        return scripts;
    }

    ScriptNameComboBox::ScriptNameComboBox(QWidget* parent)
        : QComboBox(parent)
    {
        setEditable(true);
        setInsertPolicy(QComboBox::NoInsert);
        completer()->setCompletionMode(QCompleter::PopupCompletion);
        completer()->setFilterMode(Qt::MatchContains);
        completer()->setCaseSensitivity(Qt::CaseInsensitive);

        if (g_Game->GetType() == GameType::RDR2)
        {
            lineEdit()->setPlaceholderText("full name (e.g. content/main)");
        }
        else
        {
            lineEdit()->setPlaceholderText("Script name");
        }

        RefreshScripts();
    }

    void ScriptNameComboBox::RefreshScripts()
    {
        QStringList names;
        for (const auto& [name, hashedName] : GetRunningScripts())
            names << QString::fromStdString(hashedName);

        names.removeDuplicates();
        names.sort(Qt::CaseInsensitive);

        QString text = currentText();
        blockSignals(true);
        clear();
        addItems(names);
        setCurrentIndex(-1);
        setEditText(text);
        blockSignals(false);
    }

    void ScriptNameComboBox::showPopup()
    {
        RefreshScripts();
        QComboBox::showPopup();
    }

    void ScriptNameComboBox::focusInEvent(QFocusEvent* event)
    {
        RefreshScripts();
        QComboBox::focusInEvent(event);
    }

    std::string ScriptNameComboBox::GetScriptName() const
    {
        QString text = currentText().trimmed();
        if (g_Game->GetType() != GameType::RDR2)
            return text.toStdString();

        if (text == m_ResolvedText)
            return m_ResolvedName;

        m_ResolvedText = text;
        m_ResolvedName = text.toStdString();

        // a full name already, or not running
        if (g_Game->GetThread(JOAAT(m_ResolvedName)))
            return m_ResolvedName;

        for (const auto& [name, hashedName] : GetRunningScripts())
        {
            if (QString::fromStdString(name).compare(text, Qt::CaseInsensitive) == 0)
            {
                m_ResolvedName = hashedName;
                break;
            }
        }

        return m_ResolvedName;
    }

    uint32_t ScriptNameComboBox::GetScriptHash() const
    {
        return JOAAT(GetScriptName());
    }
}