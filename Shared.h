#ifndef Shared
#define Shared

#include <boost/container/list.hpp>
#include <map>
#include <string>
#include <boost/asio.hpp>

#define MAX_PLAYERS 5
#define DATA_DELIMITER ":"
#define END_OF_TRANSMISSION "\n"
#define GAME_DATA_DELIMITER ","

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

// inbound/outbound from server side
enum Action {
    InboundStartGame,
    InboundPlayerUsedCards,
    InboundPlayerTakesCards,
    OutboundGameData,
    OutboundGameStarted
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
        int delimiterIndex = stringPacket.find(DATA_DELIMITER);
        std::string actionString = stringPacket.substr(0, delimiterIndex);
        action = (Action)atoi(actionString.c_str());
        data = stringPacket.substr(delimiterIndex + 1);
    }

    string toString() {
        return std::to_string((int)action) + DATA_DELIMITER + data;
    }
};

enum Color {// 🂠 - blank card
    SPADE,  // ♠ - green
    CLUB,   // ♣ - brown
    HEART,  // ♥ - red
    DIAMOND // ♦ - black
};

const std::map<Color, string> COLOR_STRING_VALUES = {
        { SPADE,   "♠ Zeleň"},
        { CLUB,    "♣ Žaluď"},
        { HEART,   "♥ Červeň"},
        { DIAMOND, "♦ Guľa"}
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
        { JACK,  "Dolník"},
        { QUEEN, "Horník"},
        { KING,  "Kráľ"},
        { ACE,   "Eso"}
};

struct Card {
    Color color;
    Value value;
    bool canBeUsed = false;
    bool isUsed = false;

    Card(Color colorIn, Value valueIn) {
        color = colorIn;
        value = valueIn;
    }

    Card() {

    }

    /**
     * Client-side only
     * @return String representation of this card
     */
    auto toString(int optionId);

    bool canBePlacedOn(Card& bottomCard) {
        return bottomCard.color == color || bottomCard.value == value || value == QUEEN;
    }
};

class Player {
public:
    int maxCards;
    std::vector<Card> cards;
    bool played;

    Player() {
        maxCards = 5;
        played = true;
    }

    void setHasPlayed() {
        played = true;
    }

    void setPlayersTurn() {
        played = false;
    }
};

#endif //Shared