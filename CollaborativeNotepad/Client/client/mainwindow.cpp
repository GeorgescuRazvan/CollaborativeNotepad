#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    myTextEdit = ui->notepadText;
    commandLineEdit = ui->commandLineEdit;
    connect(ui->commandLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onCommandEntered);
    myTextEdit->setReadOnly(true);
}

// aici se ia comanda din linia de comenzi si se trimite la server
void MainWindow::onCommandEntered()
{
    QString command = ui->commandLineEdit->text();
    QStringList commandParts = command.split(' ', Qt::SkipEmptyParts);

    ClientCmd cmd;

    if (!commandParts.isEmpty()) {
        cmd.command = commandParts[0].toStdString();
        if (commandParts.size() > 1) {
            cmd.text = commandParts[1].toStdString();
        }
    }

    if (cmd.command == "download")
        nameOfFile = cmd.text;
    sendClientCmd(clientSocket, cmd);
    ui->commandLineEdit->clear();
}

void MainWindow::handleUpdateText(const QString &text)
{
    myTextEdit->setPlainText(text);
}

void MainWindow::handleSetMyTextEditReadOnly(bool readOnly)
{
    if (myTextEdit) {
        myTextEdit->setReadOnly(readOnly);
    }
}

void MainWindow::handleInsertCharacterAt(int position, QChar character)
{
    if (myTextEdit) {
        QTextCursor originalCursor = myTextEdit->textCursor();
        QTextCursor cursor = originalCursor;
        cursor.setPosition(position);
        cursor.insertText(character);

        myTextEdit->setTextCursor(originalCursor);
    }
}

void MainWindow::handleDeleteCharacterAt(int position)
{
    if (myTextEdit) {
        QTextCursor cursor = myTextEdit->textCursor();
        QTextCursor tempCursor = cursor;
        tempCursor.setPosition(position);
        tempCursor.deleteChar();

        myTextEdit->setTextCursor(cursor);
    }
}

void sendClientCmd(int socket, const ClientCmd &cmd)
{
    std::string delimiter = "|%|";
    std::string serializedData = cmd.command + delimiter + cmd.text + delimiter
                                 + std::to_string(cmd.position) + delimiter
                                 + std::to_string(cmd.userSideTotalCharacters);

    send(socket, serializedData.c_str(), serializedData.size(), 0);
}

bool MainWindow::handleCreateTextFile(const std::string &filename, const std::string &content)
{
    std::ofstream fileStream(filename + nameOfFile);

    if (!fileStream.is_open()) {
        return false;
    }

    fileStream << content;
    fileStream.close();

    return true;
}

MainWindow::~MainWindow()
{
    delete ui;
}
