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
        return remainingCards.size();
    }

    Card getLastUsedCard() {
        return usedCards.back();
    }
};
struct GameData;

struct ClientConnection {
    tcp::socket* clientSocket;
    int clientId;
    GameData* gameData;
    std::queue<Packet> packetsToSend;

    ~ClientConnection() {
        delete clientSocket;
    }
};


struct GameData {
    std::map<Player, ClientConnection> playerData;
    Deck deckData;
    bool gameStarted = false;
    int activePlayerId = 0;
    pthread_mutex_t mutex;
    pthread_cond_t conditionVariableGameStarted;

    void setGameStarted() {
        gameStarted = true;
        pthread_cond_signal(&conditionVariableGameStarted);
    }

    void processAction(int playerId, Action action) {

    }
};

void* startConnection(void* args) {
    auto clientConnection = *((ClientConnection*)args);

    if (clientConnection.clientId == 0) {
        string message = readFromSocket(*clientConnection.clientSocket);
        Packet parsedPacket = Packet(message);

        if (parsedPacket.action == StartGame) {
            clientConnection.gameData->setGameStarted();
        } else {
            // what the client doing
            return nullptr;
        }
    }

    while (!clientConnection.gameData->gameStarted) {
        pthread_cond_wait(&clientConnection.gameData->conditionVariableGameStarted, &clientConnection.gameData->mutex);
    }

    while (true) {
        if (clientConnection.clientId == clientConnection.gameData->activePlayerId) {
            string message = readFromSocket(*clientConnection.clientSocket);
        }

        pthread_mutex_lock(&clientConnection.gameData->mutex);

        while (clientConnection.packetsToSend.empty()) {
            pthread_cond_wait(&clientConnection.gameData->conditionVariableGameStarted, &clientConnection.gameData->mutex);
        }

        Packet packetToSend = clientConnection.packetsToSend.front();
        clientConnection.packetsToSend.pop();
        pthread_mutex_lock(&clientConnection.gameData->mutex);

        sendToSocket(*clientConnection.clientSocket, packetToSend.toString());
        usleep(10000);
    }
}

int main() {
    GameData gameData = GameData();
    int currentPlayers = 0;
    pthread_t clientConnectionThreads[MAX_PLAYERS];
    boost::asio::io_service io_service;
    //listen for new connection
    tcp::acceptor acceptor_(io_service, tcp::endpoint(tcp::v4(), 10234));
    //waiting for connection
    while (currentPlayers < MAX_PLAYERS && !gameData.gameStarted) {
        tcp::socket* socket = new tcp::socket(io_service);
        acceptor_.accept(*socket);

        if (gameData.gameStarted) {
            socket->close();
            break;
        }

        ClientConnection clientConnection {
                socket,
                currentPlayers,
                &gameData
        };

        pthread_create(&clientConnectionThreads[currentPlayers], nullptr, startConnection, &clientConnection);
        currentPlayers++;
    }

    for (int i = 0; i < currentPlayers; i++) {
        pthread_join(clientConnectionThreads[i], nullptr);
    }
}