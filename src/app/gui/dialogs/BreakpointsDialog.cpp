#include "BreakpointsDialog.hpp"
#include "gui/widgets/ScriptNameComboBox.hpp"
#include "pipe/PipeCommands.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTableWidget>
#include <QVBoxLayout>

namespace scrDbgApp
{
    BreakpointsDialog::BreakpointsDialog(bool pauseGame, bool canChangePauseGame, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle("Breakpoints");
        resize(520, 420);
        setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);

        m_Table = new QTableWidget(this);
        m_Table->setColumnCount(2);
        m_Table->setHorizontalHeaderLabels({"Script", "Address"});
        m_Table->horizontalHeader()->setStretchLastSection(true);
        m_Table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_Table->verticalHeader()->setVisible(false);
        m_Table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_Table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_Table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_Table->setToolTip("Double-click to show a breakpoint in its script's disassembly.");
        connect(m_Table, &QTableWidget::cellDoubleClicked, this, &BreakpointsDialog::OnItemDoubleClicked);
        connect(m_Table, &QTableWidget::itemSelectionChanged, this, [this]() {
            m_Remove->setEnabled(!m_Table->selectedItems().isEmpty());
        });

        m_Remove = new QPushButton("Remove", this);
        m_Remove->setEnabled(false);
        connect(m_Remove, &QPushButton::clicked, this, &BreakpointsDialog::OnRemoveClicked);

        m_RemoveAll = new QPushButton("Remove All", this);
        connect(m_RemoveAll, &QPushButton::clicked, this, &BreakpointsDialog::OnRemoveAllClicked);

        m_PauseGame = new QCheckBox("Breakpoints pause game", this);
        m_PauseGame->setChecked(pauseGame);
        m_PauseGame->setEnabled(canChangePauseGame);
        m_PauseGame->setToolTip(canChangePauseGame ? "Choose whether a breakpoint should pause the entire game or only its script thread." : "Can't be changed while a breakpoint is active.");
        connect(m_PauseGame, &QCheckBox::toggled, this, &BreakpointsDialog::PauseGameChanged);

        auto listButtons = new QHBoxLayout();
        listButtons->addWidget(m_Remove);
        listButtons->addWidget(m_RemoveAll);
        listButtons->addStretch();
        listButtons->addWidget(m_PauseGame);

        m_Script = new ScriptNameComboBox(this);
        m_Script->lineEdit()->setPlaceholderText("Script name");

        m_Pc = new QLineEdit(this);
        m_Pc->setPlaceholderText("Hex address (PC)");
        m_Pc->setValidator(new QRegularExpressionValidator(QRegularExpression("(0[xX])?[0-9A-Fa-f]{1,8}"), m_Pc));
        connect(m_Pc, &QLineEdit::returnPressed, this, &BreakpointsDialog::OnAddClicked);

        m_ResolvedHash = new QLabel(this);
        m_ResolvedHash->setStyleSheet("color: gray;");

        m_Add = new QPushButton("Add Breakpoint", this);
        connect(m_Add, &QPushButton::clicked, this, &BreakpointsDialog::OnAddClicked);

        auto form = new QFormLayout();
        form->addRow("Script:", m_Script);
        form->addRow("", m_ResolvedHash);
        form->addRow("Address:", m_Pc);

        auto addLayout = new QVBoxLayout();
        addLayout->addLayout(form);
        addLayout->addWidget(m_Add, 0, Qt::AlignRight);

        auto addGroup = new QGroupBox("Add Breakpoint", this);
        addGroup->setLayout(addLayout);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_Table, 1);
        layout->addLayout(listButtons);
        layout->addWidget(addGroup);
        setLayout(layout);

        RefreshBreakpoints();
    }

    void BreakpointsDialog::RefreshBreakpoints()
    {
        m_Table->setRowCount(0);

        auto breakpoints = PipeCommands::GetAllBreakpoints();
        auto active = PipeCommands::GetActiveBreakpoint();

        for (const auto& [script, pc] : breakpoints)
        {
            int row = m_Table->rowCount();
            m_Table->insertRow(row);

            QString name;
            if (auto known = m_BpScNames.find(script); known != m_BpScNames.end())
                name = QString::fromStdString(known->second);
            else
                name = "<unknown>";

            auto nameItem = new QTableWidgetItem(name);
            auto pcItem = new QTableWidgetItem(QString::number(pc, 16).toUpper());
            nameItem->setData(Qt::UserRole, script);
            pcItem->setData(Qt::UserRole, pc);

            if (active.has_value() && active->first == script && active->second == pc)
            {
                QBrush brush(QColor(0xC8, 0xF0, 0xC8));
                nameItem->setBackground(brush);
                pcItem->setBackground(brush);
            }

            m_Table->setItem(row, 0, nameItem);
            m_Table->setItem(row, 1, pcItem);
        }

        m_RemoveAll->setEnabled(m_Table->rowCount() > 0);
    }

    void BreakpointsDialog::OnAddClicked()
    {
        const uint32_t hash = m_Script->GetScriptHash();
        if (!hash)
        {
            QMessageBox::warning(this, "No Script Name", "No script name provided.");
            return;
        }

        QString pcText = m_Pc->text().trimmed();
        if (pcText.startsWith("0x", Qt::CaseInsensitive))
            pcText = pcText.mid(2);

        bool ok = false;
        uint32_t pc = pcText.toUInt(&ok, 16);
        if (!ok || pcText.isEmpty())
        {
            QMessageBox::warning(this, "Add Breakpoint", "Enter a valid hex address.");
            return;
        }

        PipeCommands::SetBreakpoint(hash, pc, true);
        InsertBpScName(hash, m_Script->GetScriptName());
        RefreshBreakpoints();
        m_Script->clear();
        m_Pc->clear();
    }

    void BreakpointsDialog::OnRemoveClicked()
    {
        auto selected = m_Table->selectedItems();
        if (selected.isEmpty())
            return;

        int row = selected.first()->row();
        uint32_t script = m_Table->item(row, 0)->data(Qt::UserRole).toUInt();
        uint32_t pc = m_Table->item(row, 1)->data(Qt::UserRole).toUInt();

        PipeCommands::SetBreakpoint(script, pc, false);
        RefreshBreakpoints();
    }

    void BreakpointsDialog::OnItemDoubleClicked(int row, int column)
    {
        Q_UNUSED(column);

        uint32_t script = m_Table->item(row, 0)->data(Qt::UserRole).toUInt();
        uint32_t pc = m_Table->item(row, 1)->data(Qt::UserRole).toUInt();
        emit BreakpointDoubleClicked(script, pc);
    }

    void BreakpointsDialog::OnRemoveAllClicked()
    {
        if (QMessageBox::question(this, "Confirm", "Remove all breakpoints?") != QMessageBox::Yes)
            return;

        PipeCommands::RemoveAllBreakpoints();

        RefreshBreakpoints();
    }
}