#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QTextEdit>

#include <QObject>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern int clientSocket;
extern std::string nameOfFile;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct ClientCmd
{
    std::string command = "";
    std::string text = "";

    // specifice comenzii de editare fisier
    int position = 0;
    size_t userSideTotalCharacters = 0;
};

void sendClientCmd(int socket, const ClientCmd &cmd);

class MyTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    MyTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {}

protected: // aici dam override la triggerul cand user-ul scrie o tasta in editorul text
    void keyPressEvent(QKeyEvent *event) override
    {
        QTextCursor cursor = this->textCursor();
        int position = cursor.position();

        ClientCmd update;
        update.command = "edit_char";
        update.position = position;
        update.userSideTotalCharacters = this->toPlainText().length();

        if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
            update.command = "del_char";

            if (event->key() == Qt::Key_Backspace && position > 0) {
                update.position--;
            }
        } else {
            update.text = event->text().toStdString().c_str()[0];
        }
        sendClientCmd(clientSocket, update);

        //QTextEdit::keyPressEvent(event);
    }
};

class HelperClass : public QObject
{
    Q_OBJECT

public:
    HelperClass(QObject *parent = nullptr)
        : QObject(parent)
    {}

signals:
    void updateText(const QString &text);
    void setMyTextEditReadOnly(bool readOnly);
    void insertCharacterAt(int position, QChar character);
    void deleteCharacterAt(int position);
    void createTextFile(std::string, std::string);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    MyTextEdit *myTextEdit;
    QLineEdit *commandLineEdit;
    void giveResponseText(const QString &text);
public slots:
    void handleSetMyTextEditReadOnly(bool readOnly);
    void handleInsertCharacterAt(int position, QChar character);
    void handleDeleteCharacterAt(int position);
    bool handleCreateTextFile(const std::string &filename, const std::string &content);
    void handleUpdateText(const QString &text);
private slots:
    void onCommandEntered();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
