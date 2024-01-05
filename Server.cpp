#include "shared.h"
#include <iostream>
#include <boost/asio.hpp>
#include <unistd.h>

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::cout;
using std::endl;

string read_(tcp::socket & socket) {
    boost::asio::streambuf buf;
    boost::asio::read_until( socket, buf, "\n" );
    string data = boost::asio::buffer_cast<const char*>(buf.data());
    return data;
}

void send_(tcp::socket & socket, const string& message) {
    string msg = message + "\n";
    boost::system::error_code error;
    boost::asio::write(socket, boost::asio::buffer(msg), error);
}

struct ClientConnection {
    tcp::socket* clientSocket;
    int clientId;

    ~ClientConnection() {
        delete clientSocket;
    }
};

void* startConnection(void* args) {
    auto clientConnection = *((ClientConnection*)args);

    while (true) {
        string message = read_(*clientConnection.clientSocket);
        cout << message << endl;
        send_(*clientConnection.clientSocket, "Hello From Server! " + std::to_string(clientConnection.clientId));
        usleep(100000);
    }
}

class Deck {
private:
    boost::container::list<Card> remainingCards;
    boost::container::list<Card> usedCards;
};

struct GameData {
    boost::container::list<Player> playerData;
    Deck deckData;
};

int main() {
    Card card = Card(DIAMOND, SEVEN);
    std::cout << card.value << std::endl;
    std::cout << card.color << std::endl;




    int maxPlayers = 5;
    int currentPlayers = 0;
    pthread_t clientConnectionThreads[5];
    boost::asio::io_service io_service;
    //listen for new connection
    tcp::acceptor acceptor_(io_service, tcp::endpoint(tcp::v4(), 10234));
    //waiting for connection
    while (currentPlayers < maxPlayers) {
        tcp::socket* socket = new tcp::socket(io_service);
        acceptor_.accept(*socket);

        ClientConnection clientConnection {
                socket,
                currentPlayers
        };

        pthread_create(&clientConnectionThreads[currentPlayers], nullptr, startConnection, &clientConnection);
        currentPlayers++;
    }

    for (int i = 0; i < currentPlayers; i++) {
        pthread_join(clientConnectionThreads[i], nullptr);
    }
}