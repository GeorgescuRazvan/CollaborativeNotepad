#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

const std::string globalFilePath
    = "/Users/razvang11/Desktop/Retele/CollaborativeNotepad/Server/server/files/";

// structura unui fisier
struct FileInfo
{
    std::string content = "";

    int clientCount = 0;
    int lastModificationPos = 0;
    int lastModificationPos_type = 0;
    int client1Socket = -1;
    int client2Socket = -1;
};

// list cu toate fisierele din folder
std::map<std::string, FileInfo> fileMap;

// structura unei comenzi
struct ClientCmd
{
    std::string command = "";
    std::string text = "";

    // specifice comenzii de editare fisier
    int position = 0;
    size_t userSideTotalCharacters = 0;
};

// serializam ClientCmd si trimitem printr-un string
void sendClientCmd(int socket, const ClientCmd &cmd)
{
    std::string delimiter = "|%|";
    std::string serializedData = cmd.command + delimiter + cmd.text + delimiter
                                 + std::to_string(cmd.position) + delimiter
                                 + std::to_string(cmd.userSideTotalCharacters);

    send(socket, serializedData.c_str(), serializedData.size(), 0);
}

std::string formatFileMap(const std::map<std::string, FileInfo> &fileMap)
{
    std::string serializedMap;

    for (const auto &pair : fileMap) {
        serializedMap += pair.first + ", with " + std::to_string(pair.second.clientCount)
                         + " active writers.\n";
    }

    return serializedMap;
}

void WriteContentOnFiles()
{
    for (const auto &file : fileMap) {
        std::ofstream fileStream(globalFilePath + file.first, std::ios::out | std::ios::binary);

        if (fileStream) {
            fileStream.write(file.second.content.c_str(), file.second.content.size());

            fileStream.close();
        }
    }
}

// functia de populare lista cu fisiere
void UpdateFileMap()
{
    for (const auto &entry : std::filesystem::directory_iterator(globalFilePath)) {
        if (entry.is_regular_file()) {
            std::string fileName = entry.path().filename().string();
            if (fileName != ".DS_Store") {
                std::ifstream file(entry.path(), std::ios::binary);
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string fileContent = buffer.str();

                // Update fileMap
                if (fileMap.find(fileName) == fileMap.end()) {
                    FileInfo newFile;
                    newFile.content = fileContent;
                    fileMap[fileName] = newFile;
                } else {
                    fileMap[fileName].content = fileContent;
                }
            }
        }
    }
}

// functia de creare fisier
void CreateFile(std::string &fileName)
{
    if (fileMap.find(fileName) == fileMap.end()) {
        std::ofstream fileStream(globalFilePath + fileName);
        if (!fileStream) {
            std::cerr << "Eroare la crearea fisierului: " << fileName << std::endl;
        } else {
            std::cout << "Fisier creat cu succes: " << fileName << std::endl;
            fileStream.close();
            FileInfo newFile;
            fileMap[fileName] = newFile;
        }
    }
}

// thread-ul clientului
void StartClient(int clientSocket)
{
    char buffer[1024];
    std::string receivedData;
    while (true) {
        receivedData.clear();
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived == 0) {
            std::cout << "Client " << clientSocket << " Disconnected" << std::endl;
            break;
        }

        if (bytesReceived == -1) {
            std::cerr << "Client " << clientSocket << " Error" << std::endl;
            break;
        }

        if (bytesReceived > 0) {
            std::cout << std::endl;
            receivedData.append(buffer, bytesReceived);
            std::string delimiter = "|%|";
            size_t pos;
            ClientCmd cmd;
            std::string fileName;

            std::cout << "Date primite:" << receivedData << std::endl;
            // extractam comanda
            pos = receivedData.find(delimiter);
            cmd.command = receivedData.substr(0, pos);
            receivedData.erase(0, pos + delimiter.length());
            std::cout << "Date in cmd.command:" << cmd.command << std::endl;
            // extractam textul
            pos = receivedData.find(delimiter);
            cmd.text = receivedData.substr(0, pos);
            receivedData.erase(0, pos + delimiter.length());
            std::cout << "Date in cmd.text:" << cmd.text << std::endl << std::endl;

            // extractam pozitia si userSideTotalCharacters doar daca comanda este de edit
            if (cmd.command == "list") {
                cmd.text = formatFileMap(fileMap);
                sendClientCmd(clientSocket, cmd);
            } else if (cmd.command == "create") {
                CreateFile(cmd.text);
            } else if (cmd.command == "edit") {
                fileName = cmd.text;
                if (fileMap[fileName].clientCount < 2) {
                    fileMap[fileName].clientCount++;
                    if (fileMap[fileName].client1Socket == -1) {
                        fileMap[fileName].client1Socket = clientSocket;
                    } else if (fileMap[fileName].client2Socket == -1) {
                        fileMap[fileName].client2Socket = clientSocket;
                    }

                    cmd.text = fileMap[fileName].content;
                    sendClientCmd(clientSocket, cmd);

                    while (true) {
                        receivedData.clear();
                        memset(buffer, 0, sizeof(buffer));
                        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

                        if (bytesReceived > 0) {
                            receivedData.append(buffer, bytesReceived);
                            std::cout << "Date primite in edit:" << receivedData << std::endl;

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
                            receivedData.erase(0, pos + delimiter.length());

                            // extractam userSideTotalCharacters
                            cmd.userSideTotalCharacters = std::stoi(receivedData);
                            receivedData.clear();

                            if (cmd.command == "edit_char") {
                                /* verificam daca lungimea fisierului de pe server si a celui de pe client sunt egale,
                                 * sau daca ultima modificare exterioara clientului este mai devreme in fisier decat cea a lui*/
                                if (cmd.userSideTotalCharacters == fileMap[fileName].content.length()
                                    || fileMap[fileName].lastModificationPos >= cmd.position) {
                                    fileMap[fileName].content.insert(cmd.position, cmd.text);
                                    fileMap[fileName].lastModificationPos = cmd.position;
                                    fileMap[fileName].lastModificationPos_type
                                        = 1; // inseamna ca ultima modificare a adaugat un caracter
                                } else // daca s-a facut o modificare pe care userul nu o are si e mai devreme in fisier, schimbam (+/-1) poz. modificarii
                                    if (fileMap[fileName].lastModificationPos < cmd.position) {
                                        cmd.position
                                            += fileMap[fileName]
                                                   .lastModificationPos_type; // modificam pozitia inserarii in cmd
                                        fileMap[fileName].content.insert(cmd.position, cmd.text);
                                        fileMap[fileName].lastModificationPos = cmd.position;
                                        fileMap[fileName].lastModificationPos_type = 1;
                                    }

                                // trimitem comanda catre clientii conectati la fisier
                                if (fileMap[fileName].client1Socket != -1)
                                    sendClientCmd(fileMap[fileName].client1Socket, cmd);
                                if (fileMap[fileName].client2Socket != -1)
                                    sendClientCmd(fileMap[fileName].client2Socket, cmd);

                            } else if (cmd.command == "del_char") {
                                if (cmd.userSideTotalCharacters == fileMap[fileName].content.length()
                                    || fileMap[fileName].lastModificationPos > cmd.position) {
                                    fileMap[fileName].content.erase(cmd.position, 1);
                                    fileMap[fileName].lastModificationPos = cmd.position;
                                    fileMap[fileName].lastModificationPos_type = -1;
                                } else if (fileMap[fileName].lastModificationPos <= cmd.position) {
                                    // daca s-a facut o modificare pe care userul nu o are si e mai devreme in fisier, schimbam (+/-1) poz. modificarii
                                    cmd.position += fileMap[fileName].lastModificationPos_type;
                                    fileMap[fileName].content.erase(cmd.position, 1);
                                    fileMap[fileName].lastModificationPos = cmd.position;
                                    fileMap[fileName].lastModificationPos_type = -1;
                                }

                                // trimitem comanda catre clientii conectati la fisier
                                if (fileMap[fileName].client1Socket != -1)
                                    sendClientCmd(fileMap[fileName].client1Socket, cmd);
                                if (fileMap[fileName].client2Socket != -1)
                                    sendClientCmd(fileMap[fileName].client2Socket, cmd);

                            } else if (cmd.command == "exit") {
                                fileMap[fileName].clientCount--; // decrementam nr. de clienti
                                WriteContentOnFiles(); // scriem pe fisier schimbarile de fiecare data cand iese un client
                                if (fileMap[fileName].client1Socket
                                    == clientSocket) { // scoatem socketul din evidenta fisierului
                                    fileMap[fileName].client1Socket = -1;
                                    sendClientCmd(clientSocket, cmd);
                                } else if (fileMap[fileName].client2Socket == clientSocket) {
                                    fileMap[fileName].client2Socket = -1;
                                    sendClientCmd(clientSocket, cmd);
                                }
                                break;
                            }
                        } else if (bytesReceived == 0) {
                            fileMap[fileName].clientCount--;
                            WriteContentOnFiles();
                            std::cout << "Disconnected" << std::endl;
                            goto exit_loops;
                        } else if (bytesReceived == -1) {
                            fileMap[fileName].clientCount--;
                            WriteContentOnFiles();
                            std::cerr << "Error" << std::endl;
                            goto exit_loops;
                        }
                    }
                }
            } else if (cmd.command == "download") {
                WriteContentOnFiles();
                cmd.text = fileMap[cmd.text].content;
                sendClientCmd(clientSocket, cmd);
            }
        }
    }
exit_loops:
    close(clientSocket);
}

int main()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1234);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    listen(serverSocket, 5);

    std::cout << "Server online" << std::endl;

    UpdateFileMap();

    while (true) {
        int clientSocket = accept(serverSocket, NULL, NULL);
        std::thread clientThread(StartClient, clientSocket);
        clientThread.detach();
    }

    return 0;
}
