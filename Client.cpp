#include <boost/asio.hpp>
#include "color.hpp"
#include "Shared.h"
#include <conio.h>
#include <thread>
#include <queue>

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::cout;
using std::endl;
using std::thread;

struct ClientData {
    tcp::socket* socket;
    std::vector<int> playersCardsCount;
    Player clientPlayer;
    bool gameIsStarted = false;
    int deckCardsCount = 0;
    int activePlayerId = -1;
    int clientId = -1;
    int activeSevens = 0;
    bool isBeingSkipped = false;
    int lastPlayerToGetRidOfAllCards = -1;
    Card lastCard = Card(SPADE, SEVEN);
    std::vector<int> listOfSelectedOptionIndexes;
    std::mutex mutex;
    std::queue<Packet> inboundPackets;
    std::queue<Packet> outboundPackets;
    std::condition_variable conditionVariableInboundPackets;
    std::condition_variable conditionVariableOutboundPackets;
    std::condition_variable conditionVariableGameData;
    bool threadsRunning = true;
    bool isNewData = true;

    void reevaluateCardsThatCanBeUsed() {
        int lastIndex = -1;

        if (!listOfSelectedOptionIndexes.empty()) {
            lastIndex = listOfSelectedOptionIndexes.back();
        }

        Card currentBottomCard = lastIndex == -1 ? lastCard : clientPlayer.cards[lastIndex];

        for (Card& card : clientPlayer.cards) {
            if (card.isUsed) continue;

            if (lastPlayerToGetRidOfAllCards != -1 && card.color == HEART && card.value == SEVEN) {
                card.canBeUsed = true;
                continue;
            }

            if (isBeingSkipped && card.value == ACE) {
                card.canBeUsed = true;
                continue;
            }

            if (activePlayerId != clientId) {
                card.canBeUsed = false;
                continue;
            }

            if (activeSevens != 0) {
                card.canBeUsed = card.value == SEVEN || (card.value == JACK && card.color == SPADE);
                continue;
            }

            if (lastIndex != -1) {
                card.canBeUsed = card.value == currentBottomCard.value;
                continue;
            }

            card.canBeUsed = card.canBePlacedOn(currentBottomCard) && gameIsStarted;
        }
    }

    void setClientData(Packet& clientDataPacket) {
        // deckCardsCount, lastCard.color, lastCard.value, client.countCards, [client.cards[i].color, client.cards[i].value], otherPlayers.count, [otherPlayers[i].countCards]
        // deckCardsCount, activePlayerId, thisClientId, activeSevens, beingSkipped(0/1), lastPlayerToGetRidOfAllCards, lastCard.color, lastCard.value, client.countCards, [client.cards[i].color, client.cards[i].value], players.count, [players[i].countCards]
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

        int index = 0;
        deckCardsCount = dataSplit[index++];
        activePlayerId = dataSplit[index++];
        clientId = dataSplit[index++];
        activeSevens = dataSplit[index++];
        isBeingSkipped = dataSplit[index++] == 1;
        lastPlayerToGetRidOfAllCards = dataSplit[index++];
        lastCard = Card((Color)dataSplit[index], (Value)dataSplit[index + 1]);
        index += 2;

        int playerCardCount = dataSplit[index++];
        clientPlayer.cards.clear();

        for (int i = 0; i < playerCardCount; i++) {
            clientPlayer.cards.push_back(Card((Color)(dataSplit[index + i * 2]), (Value)(dataSplit[index + 1 + i * 2])));
        }

        int otherPlayerCount = dataSplit[index + playerCardCount * 2];
        playersCardsCount.clear();

        for (int i = 0; i < otherPlayerCount; i++) {
            playersCardsCount.push_back(dataSplit[index + playerCardCount * 2 + i + 1]);
        }

        reevaluateCardsThatCanBeUsed();
        isNewData = true;
        conditionVariableGameData.notify_one();

        lock.unlock();
    }

    void processPacket(Packet& packet) {
        switch (packet.action) {
            case OutboundGameStarted: {
                gameIsStarted = true;

                std::unique_lock<std::mutex> lock(mutex);

                isNewData = true;
                conditionVariableGameData.notify_one();

                lock.unlock();
                break;
            }
            case OutboundGameData:
                cout << "prijimam data" << endl;
                setClientData(packet);
                break;
        }
    }

    void handleInput(char input) {
        // HANDLE INPUT

        if  ( input == 8 || input == 13 || input >= '0' && input <= '9' || input >= 'a' && input <= 'w') {
            selectOptionWithIndex(int(input));
        }
        //else - sa nestane nic, pokial stlacis nieco, co nema priradenu funkcionalitu

        /*Packet packetToSend = Packet(InboundPlayerUsedCards, std::to_string(input));*/
        cout << input << endl;

        /*std::unique_lock<std::mutex> lock(mutex);

        outboundPackets.push(packetToSend);
        conditionVariableOutboundPackets.notify_one();

        lock.unlock();*/
    }

    void selectOptionWithIndex(int input) {
        switch (input) {
            case 8: {
                //stlaceny BACKSPACE
                //posli spravu na zrusenie vyberu
                deselectAll();
                return;
            }
            case 13: {
                if (!gameIsStarted) {
                    if (clientId == 0) {
                        Packet dataToSend = Packet(InboundStartGame);
                        std::unique_lock<std::mutex> lock(mutex);

                        outboundPackets.push(dataToSend);
                        conditionVariableOutboundPackets.notify_one();

                        lock.unlock();
                    }

                    break;
                }

                //stlaceny ENTER
                //posli spravu na ukoncenie kola...nejaku...nejako
                //potvrdim transakciu, teda nastavim vybratym kartam isUsed na true
                confirmSelection();
                //a premazem selected options list
                deselectAll();
                reevaluateCardsThatCanBeUsed();
                return;
            }
            case 48: {
                //stlacena 0
                // posli spravu na potiahnutie kariet z decku
                if (!gameIsStarted || clientId != activePlayerId) break;

                Packet dataToSend = Packet(InboundPlayerTakesCards);
                std::unique_lock<std::mutex> lock(mutex);

                outboundPackets.push(dataToSend);
                conditionVariableOutboundPackets.notify_one();

                lock.unlock();

                return;
            }

            //inak posli stlaceny key do listu vybratych optionov
            default: {
                std::unique_lock<std::mutex> lock(mutex);

                if (!gameIsStarted) break;

                int option;

                if (input > '0' && input <= '9') {
                    option = input - '1';
                } else {
                    option = input - 'a';
                }

                if (clientPlayer.cards.size() <= option) break;

                if (!clientPlayer.cards[option].canBeUsed) break;

                clientPlayer.cards[option].isUsed = true;
                clientPlayer.cards[option].canBeUsed = false;

                //pokial input este nie je selectnuty, respektive v liste selectnutych, pridam ho na koniec tohto listu
                if (std::find(listOfSelectedOptionIndexes.begin(), listOfSelectedOptionIndexes.end(), option) == listOfSelectedOptionIndexes.end()) {
                    listOfSelectedOptionIndexes.push_back(option);
                }

                reevaluateCardsThatCanBeUsed();
                isNewData = true;
                conditionVariableGameData.notify_one();

                lock.unlock();
                break;
            }
        }
    }

    void confirmSelection() {
        if (!gameIsStarted) return;
        //ked mam potvrdeny list optionov na selectnutie, selectnem karty im zodpovedajuce

        string cardToUse = "";

        for (int i = 0; i < listOfSelectedOptionIndexes.size(); i++) {
            cardToUse += std::to_string(listOfSelectedOptionIndexes[i]) + (i == listOfSelectedOptionIndexes.size() - 1 ? "" : GAME_DATA_DELIMITER);
        }

        Packet dataToSend = Packet(InboundPlayerUsedCards, cardToUse);
        std::unique_lock<std::mutex> lock(mutex);

        outboundPackets.push(dataToSend);
        conditionVariableOutboundPackets.notify_one();

        lock.unlock();
    }

    void deselectAll() {
        /* TODO: nie som si isty, ako presne to bude naimplementovane na strane servera, ze kedy nastavis kartam "isUsed = false"
         * takze zatial pri deselectAll len premazem list optionov, ktore chcem vybrat
         * a na samotne selectnutie musis "potvrdit transakciu" cez ENTER
         */
        listOfSelectedOptionIndexes.clear();

        std::unique_lock<std::mutex> lock(mutex);

        for (Card& card : clientPlayer.cards) {
            card.isUsed = false;
            card.canBeUsed = true;
        }

        reevaluateCardsThatCanBeUsed();
        isNewData = true;
        conditionVariableGameData.notify_one();

        lock.unlock();
    }
};

auto Card::toString(char optionId) {
    string output = (optionId == -1 ? "" : "["+string(1, optionId)+"]" + "\t") + COLOR_STRING_VALUES.at(color) + " " + VALUE_STRING_VALUES.at(value);

    switch (color) {
        case SPADE:
            if (!canBeUsed && !isUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::green_on_aqua(output);
            } else {
                return dye::green_on_black(output);
            }
        case CLUB:
            if (!canBeUsed && !isUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::yellow_on_aqua(output);
            } else {
                return dye::yellow_on_black(output);
            }
        case HEART:
            if (!canBeUsed && !isUsed) {
                return dye::grey_on_black(output);
            } else if (isUsed) {
                return dye::red_on_aqua(output);
            } else {
                return dye::red_on_black(output);
            }
        case DIAMOND:
            if (!canBeUsed && !isUsed) {
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
        system("cls");
        data.threadsRunning = false;
        cout << "Client cannot read packets: " << error.what() << endl;

        int winner = -1;

        for (int i = 0; i < data.playersCardsCount.size(); i++) {
            if (data.playersCardsCount[i] > 0) {
                if (winner != -1 && winner != -2) winner = -2;
                winner = i;
            }
        }

        cout << "Winner according to last data: " << (winner < 0 ? "cannot be determined" : "Player " + std::to_string(winner)) << endl;
        cout << "Press any key to end" << endl;
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
        std::unique_lock<std::mutex> lock(data.mutex);

        while (!data.isNewData && data.threadsRunning) {
            data.conditionVariableGameData.wait(lock);
        }

        if (!data.threadsRunning) {
            lock.unlock();
            return;
        }

        data.isNewData = false;

        system("cls");
        auto hrac = dye::green_on_black(data.activePlayerId+1);
        auto stojis = dye::grey_on_black("Aktivne ESO, stojis!");
        auto cervenaSedma = (data.lastPlayerToGetRidOfAllCards == -1 ? dye::grey_on_black("Teraz je mozne vyolizt Cervenu 7 pre vratenie hraca do hry!") : dye::red_on_black("Teraz je mozne vyolizt Cervenu 7 pre vratenie hraca do hry!"));
        auto separator = "=================================================================";
        auto poslednaVylozenaKarta = data.lastCard.toString(-1);
        auto sedmyNula = dye::grey_on_black(data.activeSevens);
        auto sedmyAktivne = dye::red_on_black(data.activeSevens);
        auto pocetAktivnychSediemNula = dye::grey_on_black("Pocet aktivnych sediem: ");
        auto pocetAktivnychSediemAktivne = dye::red_on_black("Pocet aktivnych sediem: ");
        auto poznamka = dye::grey_on_black("Pri vybrati viacerych kariet naraz budu vylozene v poradi vyberu.");
        auto potvrdenie = dye::green_on_black("Pre potvrdenie vyberu kariet stlac [ENTER].");

        if (!data.gameIsStarted && data.clientId == 0) {
            cout << "Si prvy hrac a zacinas hru! Kedykolvek stlac [ENTER] pre zacatie hry!" << endl << endl;
        }

        cout << "Teraz hra: " << hrac << endl;
        cout << stojis << endl;
        cout << cervenaSedma << endl;
        cout << separator << endl;
        cout << "Stav kariet ostatnych hracov:" << endl;
        for (int i = 0; i < data.playersCardsCount.size(); i++) {
            cout << "Player " << i+1 << ": " << data.playersCardsCount[i] << endl;
        }
        cout << separator << endl;
        cout << "Pocet kariet v decku: " << data.deckCardsCount << endl;
        cout << "Posledna vylozena karta: " << poslednaVylozenaKarta << endl;
        cout << separator << endl;
        cout << "Tvoje karty: " << endl;
        for (int i = 0; i < data.clientPlayer.cards.size(); ++i) {
            if (i < 9) {
                cout << data.clientPlayer.cards[i].toString(i+1 + '0') << endl;
            } else {
                cout << data.clientPlayer.cards[i].toString('a' + (i - 10)) << endl;
            }
        }
        cout << endl;
        cout << ((data.gameIsStarted && data.clientId == data.activePlayerId) ? dye::white_on_black("[0]\tPotiahni si kartu") : dye::grey_on_black("[0]\tPotiahni si kartu")) << endl << endl;
        if (data.activeSevens < 1) {
            cout << pocetAktivnychSediemNula << sedmyNula << endl;
        } else {
            cout << pocetAktivnychSediemAktivne << sedmyAktivne << endl;
        }

        cout << separator << endl;

        cout << "Stlac tlacidlo zobrazene v zatvorke pre vybratie danej karty." << endl;
        cout << "Karty na sedom pozadi vybrat nemozes." << endl;
        cout << "Kariet mozes naraz vybrat viac pokial je to mozne." << endl;
        cout << poznamka << endl;
        cout << "Pre zrusenie vyberu stlac [BACKSPACE]." << endl;
        cout << potvrdenie << endl;
        cout << "Pre potiahnutie karty stlac [0]." << endl;

        lock.unlock();
        // OUTPUT TO CONSOLE, lock mutex when reading clientData
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        string ip;
        cout << "Zadaj IP adresu servera: ";
        std::cin >> ip;

        try {
            socket->connect(tcp::endpoint(ip::address::from_string(ip), 10234)); //connection
        } catch (std::exception& error) {
            cout << "There was a problem connecting to the server." << endl;
            delete socket;
            return 1;
        }

        thread inboundThread = thread(startInboundThread, std::ref(gameData));
        thread outboundThread = thread(startOutboundThread, std::ref(gameData));
        thread consoleOutputThread = thread(startConsoleOutputThread, std::ref(gameData));
        thread inboundPacketHandler = thread(startInboundPacketHandler, std::ref(gameData));
        char c = getch();

        while (gameData.threadsRunning) {
            gameData.handleInput(c);
            c = getch();
        }

        gameData.conditionVariableOutboundPackets.notify_one();
        gameData.conditionVariableInboundPackets.notify_one();
        gameData.conditionVariableGameData.notify_one();

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
