#define SERVER
#include "Shared.h"
#include <iostream>
#include <vector>
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

    void shuffleDeck() {
        static std::random_device randomDevice = std::random_device();
        static std::default_random_engine rng = std::default_random_engine(randomDevice());
        std::shuffle(std::begin(remainingCards), std::end(remainingCards), rng);
    }

    void putUsedToRemaining() {
        Card& lastRemaining = usedCards.back();
        usedCards.pop_back();

        for (Card& card : usedCards) {
            remainingCards.push_back(card);
        }

        shuffleDeck();
        usedCards.clear();
        usedCards.push_back(lastRemaining);
    }

public:
    Deck() {
        fillDeck();
        Card& firstCard = remainingCards.back();
        remainingCards.pop_back();
        usedCards.push_back(firstCard);
    }

    void fillDeck() {
        remainingCards.clear();
        usedCards.clear();

        for (int color = SPADE; color <= DIAMOND; color++) {
            for (int value = SEVEN; value <= ACE; value++) {
                remainingCards.push_back(Card((Color)color, (Value)value));
            }
        }

        shuffleDeck();
    }

    void playCard(Card& cardToPlay) {
        usedCards.push_back(cardToPlay);
    }

    void takeCard(Player& player, int amount = 1) {
        if (remainingCards.size() < amount) {
            putUsedToRemaining();
        }

        for (int i = 0; i < amount && !remainingCards.empty(); i++) {
            Card& card = remainingCards.back();
            remainingCards.pop_back();
            player.cards.push_back(card);
        }
    }

    int getNumberOfRemainingCards() {
        return (int)remainingCards.size();
    }

    Card& getLastUsedCard() {
        return usedCards.back();
    }
};
struct GameData;

struct ClientConnection {
    tcp::socket* clientSocket;
    int clientId;
    GameData* gameData;
    std::queue<Packet> outboundPackets;
    pthread_cond_t conditionVariableOutboundPackets;
};

struct GameData {
    std::vector<Player> playerData;
    std::vector<ClientConnection*> clientConnections;
    Deck deckData;
    bool gameStarted = false;
    bool serverRunning = true;
    int activePlayerId = 0;
    pthread_mutex_t mutex;
    pthread_cond_t conditionVariableInboundPackets;
    std::queue<Packet> inboundPackets;
    int activeSevens = 0;
    int activeAces = 0;
    int lastPlayerToGetRidOfAllCards = -1;

    void setGameStarted() {
        gameStarted = true;
        Packet packetGameStarted = Packet(OutboundGameStarted);

    }

    bool canBePlayed(Card& card) {
        Card& lastPlayedCard = deckData.getLastUsedCard();

        if ((activeSevens != 0) && (!(card.color == SPADE && card.value == JACK) || card.value != SEVEN)) return false;

        if (card.color == SPADE && card.value == JACK && (activeSevens != 0)) return true;
        if (card.color == HEART && card.value == SEVEN && lastPlayerToGetRidOfAllCards != -1) return true;

        return card.canBePlacedOn(lastPlayedCard);
    }

    void progressActivePlayer() {
        activePlayerId++;

        if (activePlayerId >= playerData.size()) {
            lastPlayerToGetRidOfAllCards = -1;
            activePlayerId = 0;
        }

        if (playerData[activePlayerId].cards.empty()) {
            progressActivePlayer();
        } else {
            playerData[activePlayerId].played = false;

            if (getLastPlayerWithCards() != -1) {
                //endGame();
            }
        }
    }

    int getLastPlayerWithCards() {
        int playerWithCards = -1;

        for (int i = 0; i < playerData.size(); i++) {
            if (!playerData[i].cards.empty()) {
                if (playerWithCards != -1) {
                    return -1;
                }

                playerWithCards = i;
            }
        }

        return playerWithCards;
    }

    void endRound(int losingPlayer) {
        playerData[losingPlayer].maxCards--;
        deckData.fillDeck();

        for (Player& player : playerData) {
            preparePlayerForRound(player);
        }
    }

    void playerUsedCard(Packet& packetIn) {
        if (!gameStarted) return;
        Player& player = playerData[packetIn.playerId];
        int cardIndex = std::stoi(packetIn.data);

        if (cardIndex >= player.cards.size()) return; // TODO: more cards of the same VALUE can be played

        Card& cardToPlay = player.cards[cardIndex];

        if (packetIn.playerId != activePlayerId && !(activeAces > 0 && cardToPlay.value == ACE)) return; // TODO: restrict to players in range
        if (!canBePlayed(cardToPlay)) return;

        player.cards.erase(player.cards.begin() + cardIndex);

        if (cardToPlay.value == ACE) {
            activeAces++;
            progressActivePlayer();
        }

        if (cardToPlay.value == SEVEN) activeSevens++;

        deckData.playCard(cardToPlay);
        player.setHasPlayed();
        sendDataToAllPlayers();
    }

    void playerTakesCards(Packet& packetIn) {
        if (!gameStarted) return;
        if (packetIn.playerId != activePlayerId) return;

        Player& player = playerData[packetIn.playerId];
        int cardsToTake = std::stoi(packetIn.data);

        if ((activeSevens != 0) && activeSevens * 3 != cardsToTake) return;

        deckData.takeCard(player, cardsToTake);
        player.setHasPlayed();
        sendDataToAllPlayers();
    }

    void playerEndsTurn(Packet& packetIn) {
        if (!gameStarted) return;
        Player& player = playerData[packetIn.playerId];

        if (!player.played || packetIn.playerId != activePlayerId) return;

        int lastPlayerWithCards = getLastPlayerWithCards();

        if (lastPlayerWithCards != -1) {
            endRound(lastPlayerWithCards);
        } else {
            progressActivePlayer();
        }

        sendDataToAllPlayers();
    }

    void processPacket(Packet& packet) {
        cout << "Player with id " << packet.playerId << " sent a packet with action id " << packet.action << " and data " << packet.data << endl;

        switch (packet.action) {
            case InboundStartGame:
                setGameStarted();
                break;
            case InboundPlayerUsedCards:
                playerUsedCard(packet);
                break;
            case InboundPlayerTakesCards:
                playerTakesCards(packet);
                break;
            default:
                break;
        }
    }

    void preparePlayerForRound(Player& player) {
        deckData.takeCard(player, player.maxCards);
    }

    void createNewPlayer() {
        Player newPlayer = Player();
        preparePlayerForRound(newPlayer);
        playerData.push_back(newPlayer);
        sendDataToAllPlayers();
    }

    void sendDataToAllPlayers() {
        for (ClientConnection* clientConnection : clientConnections) {
            int isBeingSkipped = 0;
            Player& thisPlayer = playerData[clientConnection->clientId];
            string currentPlayerData = std::to_string(thisPlayer.cards.size());

            for (Card& card : thisPlayer.cards) {
                currentPlayerData += GAME_DATA_DELIMITER + std::to_string(card.color) + GAME_DATA_DELIMITER + std::to_string(card.value);
            }

            string playersCardsCount = std::to_string(playerData.size());

            for (Player& player : playerData) {
                playersCardsCount += GAME_DATA_DELIMITER + std::to_string(player.cards.size());
            }

            Card& lastCard = deckData.getLastUsedCard();

            string packetData =
                    std::to_string(deckData.getNumberOfRemainingCards()) + GAME_DATA_DELIMITER +
                    std::to_string(activePlayerId) + GAME_DATA_DELIMITER +
                    std::to_string(clientConnection->clientId) + GAME_DATA_DELIMITER +
                    std::to_string(activeSevens) + GAME_DATA_DELIMITER +
                    std::to_string(isBeingSkipped) + GAME_DATA_DELIMITER +
                    std::to_string(lastCard.color) + GAME_DATA_DELIMITER +
                    std::to_string(lastCard.value) + GAME_DATA_DELIMITER +
                    currentPlayerData + GAME_DATA_DELIMITER +
                    playersCardsCount;

            Packet packetToSend = Packet(OutboundGameData, packetData);
            packetToSend.playerId = -1;

            pthread_mutex_lock(&clientConnection->gameData->mutex);

            clientConnection->outboundPackets.push(packetToSend);

            pthread_cond_signal(&clientConnection->conditionVariableOutboundPackets);
            pthread_mutex_unlock(&clientConnection->gameData->mutex);
        }
    }
};

void* startInboundPacketHandler(void* args) {
    auto gameData = ((GameData*)args);

    try {
        while (gameData->serverRunning) {
            pthread_mutex_lock(&gameData->mutex);

            while (gameData->inboundPackets.empty() && gameData->serverRunning) {
                pthread_cond_wait(&gameData->conditionVariableInboundPackets, &gameData->mutex);
            }

            if (gameData->inboundPackets.empty() && !gameData->serverRunning) {
                pthread_cond_signal(&gameData->conditionVariableInboundPackets);
                pthread_mutex_unlock(&gameData->mutex);
                return nullptr;
            }

            Packet packetToProcess = gameData->inboundPackets.front();
            gameData->inboundPackets.pop();

            pthread_cond_signal(&gameData->conditionVariableInboundPackets);
            pthread_mutex_unlock(&gameData->mutex);

            try {
                gameData->processPacket(packetToProcess);
            } catch (std::exception& e) {
                cout << "Cannot process packet, most likely the server received malformed data from client with id " << packetToProcess.playerId << ": " << e.what() << endl;
            }
        }
    } catch (boost::system::system_error &error) {
        cout << "Server cannot process inbound packet: " << error.what() << endl;
        return nullptr;
    }

    return nullptr;
}

void* startInboundThread(void* args) {
    auto clientConnection = ((ClientConnection*)args);

    try {
        while (clientConnection->gameData->serverRunning) {
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
        // TODO: handle disconnect
        return nullptr;
    }

    return nullptr;
}

void* startOutboundThread(void* args) {
    auto clientConnection = ((ClientConnection*)args);

    try {
        while (clientConnection->gameData->serverRunning) {
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

    return nullptr;
}

void* startConnection(void* args) {
    auto clientConnection = ((ClientConnection*)args);

    clientConnection->gameData->clientConnections.push_back(clientConnection);
    clientConnection->gameData->createNewPlayer();

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
    tcp::acceptor acceptor_(io_service, tcp::endpoint(tcp::v4(), 10234));

    while (currentPlayers < MAX_PLAYERS && !gameData.gameStarted) {
        auto socket = new tcp::socket(io_service); // TODO: I FUCKING LOVE POINTERS, DELETE SOMEHOW ATER BECAUSE MEMORY LEAKS BAD, GARBAGE COLLECTOR BAD AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA FUCK YOU C++
        acceptor_.accept(*socket);
        cout << "A client with id " << currentPlayers <<  " connected to the server!" << endl;

        if (gameData.gameStarted) {
            socket->close();
            break;
        }

        auto clientConnection  = new ClientConnection { // TODO: fix memory leak
                socket,
                currentPlayers,
                &gameData
        };

        pthread_create(&clientConnectionThreads[currentPlayers], nullptr, startConnection, clientConnection);
        currentPlayers++;
    }

    for (int i = 0; i < currentPlayers; i++) {
        pthread_join(clientConnectionThreads[i], nullptr);
    }

    pthread_cond_destroy(&gameData.conditionVariableInboundPackets);
    pthread_mutex_destroy(&gameData.mutex);
}
