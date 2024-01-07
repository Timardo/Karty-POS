#include <boost/asio.hpp>
#include "color.hpp"
#include "Shared.h"
#include <conio.h>
#include <thread>
#include <queue>
#include "MemoryLeakChecker.h"

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::cout;
using std::endl;
using std::thread;

class ClientData {
public:
    tcp::socket* socket;
    std::vector<int> otherPlayers;
    Player clientPlayer;
    int deckCardsCount = 0;
    int activePlayerId = -1;
    int clientId = -1;
    int activeSevens = 0;
    bool isBeingSkipped = false;
    Card lastCard;
    std::mutex mutex;
    std::queue<Packet> inboundPackets;
    std::queue<Packet> outboundPackets;
    std::condition_variable conditionVariableInboundPackets;
    std::condition_variable conditionVariableOutboundPackets;
    bool threadsRunning = true;

    void setClientData(Packet& clientDataPacket) {
        // deckCardsCount, lastCard.color, lastCard.value, player.countCards, [player.cards[i].color, player.cards[i].value], otherPlayers.count, [otherPlayer[i].countCards]
        string gameDataString = clientDataPacket.data;
        std::vector<int> dataSplit;
        int delimeterIndex = 0;
        int delimeterIndex2 = (int)gameDataString.find(GAME_DATA_DELIMITER);

        do {
            dataSplit.push_back(std::stoi(gameDataString.substr(delimeterIndex, delimeterIndex2 - delimeterIndex)));
            delimeterIndex = delimeterIndex2;
            delimeterIndex2 = (int)gameDataString.find(GAME_DATA_DELIMITER, ++delimeterIndex);
        } while (delimeterIndex != 0);

        std::unique_lock<std::mutex> lock(mutex);

        deckCardsCount = dataSplit[0];
        lastCard = Card((Color)dataSplit[1], (Value)dataSplit[2]);
        int playerCardCount = dataSplit[3];
        clientPlayer.cards.clear();

        for (int i = 0; i < playerCardCount; i++) {
            clientPlayer.cards.push_back(Card((Color)(dataSplit[4] + i * 2), (Value)(dataSplit[5] + i * 2)));
        }

        int otherPlayerCount = dataSplit[4 + playerCardCount * 2];
        otherPlayers.clear();

        for (int i = 0; i < otherPlayerCount; i++) {
            otherPlayers.push_back(dataSplit[dataSplit[4 + playerCardCount * 2 + i + 1]]);
        }

        lock.unlock();
    }

    void processPacket(Packet& packet) {
        switch (packet.action) {
            case OutboundGameStarted:
                // start the game
                break;
            case OutboundGameData:
                setClientData(packet);
                break;
        }
    }

    void handleInput(char input) {
        // HANDLE INPUT

        //Packet packetToSend = Packet(...);

        std::unique_lock<std::mutex> lock(mutex);

        //outboundPackets.push(packetToSend);
        conditionVariableOutboundPackets.notify_one();

        lock.unlock();
    }
};

auto Card::toString(int optionId) {
    string output = (optionId == -1 ? "" : std::to_string(optionId) + "    ") + COLOR_STRING_VALUES.at(color) + " " + VALUE_STRING_VALUES.at(value);

    switch (color) {
        case SPADE:
            if (!canBeUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::green_on_aqua(output);
            } else {
                return dye::green_on_black(output);
            }
        case CLUB:
            if (!canBeUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::yellow_on_aqua(output);
            } else {
                return dye::yellow_on_black(output);
            }
        case HEART:
            if (!canBeUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::red_on_aqua(output);
            } else {
                return dye::red_on_black(output);
            }
        case DIAMOND:
            if (!canBeUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::white_on_aqua(output);
            } else {
                return dye::white_on_black(output);
            }
    }
}

void startInboundThread(ClientData& data) {
    try {
        while (data.threadsRunning) {
            string message = readFromSocket(*data.socket);
            Packet receivedPacket = Packet(message);

            std::unique_lock<std::mutex> lock(data.mutex);

            data.inboundPackets.push(receivedPacket);
            data.conditionVariableInboundPackets.notify_one();

            lock.unlock();
        }
    } catch (boost::system::system_error& error) {
        cout << "Client cannot read packers: " << error.what() << endl;
        return;
    }
}

void startOutboundThread(ClientData& data) {
    while (data.threadsRunning) {
        std::unique_lock<std::mutex> lock(data.mutex);

        while (data.outboundPackets.empty() && data.threadsRunning) {
            data.conditionVariableOutboundPackets.wait(lock);
        }

        if (data.outboundPackets.empty() && !data.threadsRunning) return;

        Packet packetToSend = data.outboundPackets.front();
        data.outboundPackets.pop();

        lock.unlock();

        writeToSocket(*data.socket, packetToSend.toString());
    }
}

void startConsoleOutputThread(ClientData& data) {
    while (data.threadsRunning) {

        // OUTPUT TO CONSOLE, lock mutex when reading clientData
        cout << data.clientPlayer.cards[0].toString(1) << endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void startInboundPacketHandler(ClientData& data) {
    while (data.threadsRunning) {
        std::unique_lock<std::mutex> lock(data.mutex);

        while (data.inboundPackets.empty() && data.threadsRunning) {
            data.conditionVariableInboundPackets.wait(lock);
        }

        if (data.inboundPackets.empty() && !data.threadsRunning) return;

        Packet packetToProcess = data.inboundPackets.front();
        data.inboundPackets.pop();

        lock.unlock();

        data.processPacket(packetToProcess);
    }
}

int main() {
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        ClientData gameData;
        io_service io_service;
        tcp::socket *socket = new tcp::socket(io_service); //socket creation
        gameData.socket = socket;

        thread inboundThread = thread(startInboundThread, std::ref(gameData));
        thread outboundThread = thread(startOutboundThread, std::ref(gameData));
        thread consoleOutputThread = thread(startConsoleOutputThread, std::ref(gameData));
        thread inboundPacketHandler = thread(startInboundPacketHandler, std::ref(gameData));

        while (gameData.threadsRunning) {
            char c = getch();
            gameData.handleInput(c);
        }

        gameData.conditionVariableOutboundPackets.notify_one();
        gameData.conditionVariableInboundPackets.notify_one();

        socket->cancel();
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        socket->close();

        inboundThread.join();
        outboundThread.join();
        consoleOutputThread.join();
        inboundPacketHandler.join();

        delete socket;
    }

    return 0;
}
