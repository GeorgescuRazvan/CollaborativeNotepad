#include <QApplication>
#include <QThread>
#include "mainwindow.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

QThread *serverThread;
MainWindow *mainWindow = nullptr;
int clientSocket;
HelperClass *helper = new HelperClass();
std::string nameOfFile;
void startClient()
{
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Nu s-a putut crea socket-ul client" << std::endl;
        return;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1234);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Conectarea la server a esuat" << std::endl;
        return;
    }

    char buffer[1024];
    std::string receivedData;

    while (true) {
        memset(buffer, 0, sizeof(buffer));

        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived == 0) {
            break;
        }

        if (bytesReceived == -1) {
            break;
        }

        if (bytesReceived > 0) {
            receivedData.clear();
            receivedData.append(buffer, bytesReceived);
            std::string delimiter = "|%|";
            size_t pos;
            ClientCmd cmd;
            std::string fileName;

            // extractam comanda
            pos = receivedData.find(delimiter);
            cmd.command = receivedData.substr(0, pos);
            receivedData.erase(0, pos + delimiter.length());

            // extractam textul
            pos = receivedData.find(delimiter);
            cmd.text = receivedData.substr(0, pos);
            receivedData.erase(0, pos + delimiter.length());
            // extractam pozitia doar daca comanda este de edit

            if (cmd.command == "list") {
                emit helper->updateText(QString::fromStdString(cmd.text));
            } else if (cmd.command == "create") {
                emit helper->updateText(QString::fromStdString(cmd.text));

            } else if (cmd.command == "edit") {
                emit helper->setMyTextEditReadOnly(false);
                emit helper->updateText(QString::fromStdString(cmd.text));

                while (true) {
                    receivedData.clear();
                    memset(buffer, 0, sizeof(buffer));
                    bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

                    if (bytesReceived > 0) {
                        receivedData.append(buffer, bytesReceived);

                        // extractam comanda
                        pos = receivedData.find(delimiter);
                        cmd.command = receivedData.substr(0, pos);
                        receivedData.erase(0, pos + delimiter.length());

                        // extractam textul
                        pos = receivedData.find(delimiter);
                        cmd.text = receivedData.substr(0, pos);
                        receivedData.erase(0, pos + delimiter.length());

                        // extractam pozitia
                        pos = receivedData.find(delimiter);
                        cmd.position = std::stoi(receivedData.substr(0, pos));
                        receivedData.clear();
                        if (cmd.command == "edit_char") {
                            emit helper->insertCharacterAt(cmd.position, QChar(cmd.text[0]));
                        } else if (cmd.command == "del_char") {
                            emit helper->deleteCharacterAt(cmd.position);
                        } else if (cmd.command == "exit") {
                            emit helper->setMyTextEditReadOnly(true);
                            emit helper->updateText(QString::fromStdString(""));
                            break;
                        }
                    } else if (bytesReceived == 0) {
                        std::cout << "Disconnected" << std::endl;
                        goto exit;
                    } else if (bytesReceived == -1) {
                        std::cerr << "Error" << std::endl;
                        goto exit;
                    }
                }
            } else if (cmd.command == "download") {
                emit helper->createTextFile("/Users/razvang11/Desktop/Retele/DOWNLOAD/", cmd.text);
            }
        }
    }
exit:

    close(clientSocket);
    serverThread->quit();
    serverThread->wait();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    mainWindow = &w;
    w.show();

    serverThread = QThread::create(startClient);
    serverThread->start();

    QObject::connect(helper,
                     &HelperClass::updateText,
                     &w,
                     &MainWindow::handleUpdateText,
                     Qt::QueuedConnection);
    QObject::connect(helper,
                     &HelperClass::setMyTextEditReadOnly,
                     &w,
                     &MainWindow::handleSetMyTextEditReadOnly,
                     Qt::QueuedConnection);
    QObject::connect(helper,
                     &HelperClass::insertCharacterAt,
                     &w,
                     &MainWindow::handleInsertCharacterAt,
                     Qt::QueuedConnection);
    QObject::connect(helper,
                     &HelperClass::deleteCharacterAt,
                     &w,
                     &MainWindow::handleDeleteCharacterAt,
                     Qt::QueuedConnection);
    QObject::connect(helper,
                     &HelperClass::createTextFile,
                     &w,
                     &MainWindow::handleCreateTextFile,
                     Qt::QueuedConnection);

    return a.exec();
}
