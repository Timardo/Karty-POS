#ifndef Shared
#define Shared

#include <boost/container/list.hpp>
#include <map>
#include <string>
#include <boost/asio.hpp>

#define MAX_PLAYERS 5
#define PACKET_DELIMITER ":"
#define END_OF_TRANSMISSION "\n"

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::endl;

string readFromSocket(tcp::socket& socket) {
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, END_OF_TRANSMISSION);
    string data = boost::asio::buffer_cast<const char*>(buf.data());
    return data;
}

void writeToSocket(tcp::socket& socket, const string& message) {
    string msg = message + END_OF_TRANSMISSION;
    boost::system::error_code error;
    boost::asio::write(socket, boost::asio::buffer(msg), error);
}

enum Action {
    StartGame,
    OtherPlayerAction,
    PlayerAction,
    DataNumberOfPlayers,
    Data
};

struct Packet {
    Action action;
    string data;
    int playerId;

    Packet(Action actionIn, string dataIn = "-") {
        action = actionIn;
        data = dataIn;
    }

    Packet(string stringPacket) {
        int delimiterIndex = stringPacket.find(PACKET_DELIMITER);
        std::string actionString = stringPacket.substr(0, delimiterIndex);
        action = (Action)atoi(actionString.c_str());
        data = stringPacket.substr(delimiterIndex + 1);
    }

    string toString() {
        return std::to_string((int)action) + PACKET_DELIMITER + data;
    }
};

enum Color {// 🂠 - blank card
    SPADE,  // ♠ - black
    CLUB,   // ♣ - black
    HEART,  // ♥ - red
    DIAMOND // ♦ - red
};

const std::map<Color, string> COLOR_STRING_VALUES = {
        { SPADE,   "♠"},
        { CLUB,    "♣"},
        { HEART,   "♥"},
        { DIAMOND, "♦"}
};

enum Value {
    SEVEN, // 7
    EIGHT, // 8
    NINE,  // 9
    TEN,   // 10
    JACK,  // J
    QUEEN, // Q
    KING,  // K
    ACE    // A
};

const std::map<Value, string> VALUE_STRING_VALUES = {
        { SEVEN, "7"},
        { EIGHT, "8"},
        { NINE,  "9"},
        { TEN,   "10"},
        { JACK,  "J"},
        { QUEEN, "Q"},
        { KING,  "K"},
        { ACE,   "A"}
};

class Card {
public:
    Color color;
    Value value;

public:
    Card(Color colorIn, Value valueIn) {
        color = colorIn;
        value = valueIn;
    }

    Card() {
        color = SPADE;
        value = SEVEN;
    }

    /**
     * Client-side only
     * @return String representation of this card
     */
    auto toString();

    bool canBePlacedOn(Card& bottomCard) {
        return bottomCard.color == color || bottomCard.value == value || value == QUEEN;
    }
};

class Player {
public:
    int maxCards;
    boost::container::list<Card> cards;

    Player() {
        maxCards = 5;
    }
};

struct ClientData {
    boost::container::list<Player> otherPlayers;
    Player clientPlayer;
    int deckCards;
    Card lastCard;

    ClientData() {
        deckCards = 0;
    }
};

#endif //Shared