#define SERVER
#include "shared.h"
#include <iostream>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <random>
#include <queue>

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::cout;
using std::endl;

class Deck {
private:
    std::vector<Card> remainingCards;
    std::vector<Card> usedCards;

    void fillDeck() {
        for (int color = SPADE; color <= DIAMOND; color++) {
            for (int value = SEVEN; value <= ACE; value++) {
                remainingCards.push_back(Card((Color)color, (Value)value));
            }
        }

        shuffleDeck();
    }

    void shuffleDeck() {
        static std::random_device randomDevice = std::random_device();
        static std::default_random_engine rng = std::default_random_engine(randomDevice());
        std::shuffle(std::begin(remainingCards), std::end(remainingCards), rng);
    }

    void putUsedToRemaining() {
        Card lastRemaining = usedCards.back();
        usedCards.pop_back();

        for (Card card : usedCards) {
            remainingCards.push_back(card);
        }

        shuffleDeck();
        usedCards.clear();
        usedCards.push_back(lastRemaining);
    }

public:
    Deck() {
        fillDeck();
        Card firstCard = remainingCards.back();
        remainingCards.pop_back();
        usedCards.push_back(firstCard);
    }

    void playCard(Card& cardToPlay) {
        usedCards.push_back(cardToPlay);
    }

    void takeCard(Player& player, int amount = 1) {
        if (remainingCards.size() < amount) {
            putUsedToRemaining();
        }

        for (int i = 0; i < amount && !remainingCards.empty(); i++) {
            Card card = remainingCards.back();
            remainingCards.pop_back();
            player.cards.push_back(card);
        }
    }

    int getNumberOfRemainingCards() {
        return (int)remainingCards.size();
    }

    Card getLastUsedCard() {
        return usedCards.back();
    }
};
struct GameData;

struct ClientConnection {
    tcp::socket* clientSocket;
    int clientId;
    int maxCards;
    GameData* gameData;
    std::queue<Packet> outboundPackets;
    pthread_cond_t conditionVariableOutboundPackets;
};

struct GameData {
    std::vector<Player> playerData;
    std::vector<ClientConnection*> clientConnections;
    Deck deckData;
    bool gameStarted = false;
    int activePlayerId = 0;
    pthread_mutex_t mutex;
    pthread_cond_t conditionVariableInboundPackets;
    std::queue<Packet> inboundPackets;

    void setGameStarted() {
        gameStarted = true;
    }

    void processPacket(Packet& packet) {
        cout << "Player with id " << packet.playerId << " sent a packet with action id " << packet.action << " and data " << packet.data << endl;

        switch (packet.action) {
            case StartGame:
                // do stuff...
                break;
        }
    }

    void createNewPlayer() {

    }

    void sendDataToAllPlayers() {

    }
};

void* startInboundPacketHandler(void* args) {
    auto gameData = ((GameData*)args);

    try {
        while (true) {
            pthread_mutex_lock(&gameData->mutex);

            while (gameData->inboundPackets.empty()) {
                pthread_cond_wait(&gameData->conditionVariableInboundPackets, &gameData->mutex);
            }

            Packet packetToProcess = gameData->inboundPackets.front();
            gameData->inboundPackets.pop();

            pthread_cond_signal(&gameData->conditionVariableInboundPackets);
            pthread_mutex_unlock(&gameData->mutex);

            gameData->processPacket(packetToProcess);
        }
    } catch (boost::system::system_error &error) {
        cout << "Server cannot process inbound packet: " << error.what() << endl;
        return nullptr;
    }
}

void* startInboundThread(void* args) {
    auto clientConnection = ((ClientConnection*)args);

    try {
        while (true) {
            string messageReceived = readFromSocket(*clientConnection->clientSocket);
            Packet packetReceived = Packet(messageReceived);
            packetReceived.playerId = clientConnection->clientId;

            pthread_mutex_lock(&clientConnection->gameData->mutex);

            clientConnection->gameData->inboundPackets.push(packetReceived);

            pthread_cond_signal(&clientConnection->gameData->conditionVariableInboundPackets);
            pthread_mutex_unlock(&clientConnection->gameData->mutex);
        }
    } catch (boost::system::system_error &error) {
        cout << "Client " << clientConnection->clientId << " was unable to read from socket: " << error.what() << endl;
        return nullptr;
    }
}

void* startOutboundThread(void* args) {
    auto clientConnection = ((ClientConnection*)args);

    try {
        while (true) {
            pthread_mutex_lock(&clientConnection->gameData->mutex);

            while (clientConnection->outboundPackets.empty()) {
                pthread_cond_wait(&clientConnection->conditionVariableOutboundPackets, &clientConnection->gameData->mutex);
            }

            Packet packetToSend = clientConnection->outboundPackets.front();
            clientConnection->outboundPackets.pop();

            pthread_cond_signal(&clientConnection->conditionVariableOutboundPackets);
            pthread_mutex_unlock(&clientConnection->gameData->mutex);

            writeToSocket(*clientConnection->clientSocket, packetToSend.toString());
        }
    } catch (boost::system::system_error &error) {
        cout << "Client " << clientConnection->clientId << " was unable to write to socket: " << error.what() << endl;
        return nullptr;
    }
}

void* startConnection(void* args) {
    auto clientConnection = ((ClientConnection*)args);
    pthread_t inboundThread;
    pthread_t outboundThread;
    pthread_cond_init(&clientConnection->conditionVariableOutboundPackets, nullptr);

    pthread_create(&inboundThread, nullptr, startInboundThread, clientConnection);
    pthread_create(&outboundThread, nullptr, startOutboundThread, clientConnection);

    pthread_join(inboundThread, nullptr);
    pthread_join(outboundThread, nullptr);

    pthread_cond_destroy(&clientConnection->conditionVariableOutboundPackets);
    return nullptr;
}

int main() {
    GameData gameData = GameData();

    pthread_mutex_init(&gameData.mutex, nullptr);
    pthread_cond_init(&gameData.conditionVariableInboundPackets, nullptr);

    pthread_t clientConnectionThreads[MAX_PLAYERS];
    pthread_t inboundPacketHandler;
    pthread_create(&inboundPacketHandler, nullptr, startInboundPacketHandler, &gameData);

    int currentPlayers = 0;
    boost::asio::io_service io_service;
    //listen for new connection
    tcp::acceptor acceptor_(io_service, tcp::endpoint(tcp::v4(), 10234));
    //waiting for connection
    while (currentPlayers < MAX_PLAYERS && !gameData.gameStarted) {
        tcp::socket* socket = new tcp::socket(io_service); // TODO: I FUCKING LOVE POINTERS, DELETE SOMEHOW ATER BECAUSE MEMORY LEAKS BAD, GARBAGE COLLECTOR BAD AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA FUCK YOU C++
        acceptor_.accept(*socket);

        cout << "A client with id " << currentPlayers <<  " connected to the server!" << endl;

        if (gameData.gameStarted) {
            socket->close();
            break;
        }

        ClientConnection clientConnection {
                socket,
                currentPlayers,
                5,
                &gameData
        };

        pthread_create(&clientConnectionThreads[currentPlayers], nullptr, startConnection, &clientConnection);
        currentPlayers++;
    }

    for (int i = 0; i < currentPlayers; i++) {
        pthread_join(clientConnectionThreads[i], nullptr);
    }

    pthread_cond_destroy(&gameData.conditionVariableInboundPackets);
    pthread_mutex_destroy(&gameData.mutex);
}
