#ifndef Shared
#define Shared

#include <boost/container/list.hpp>
#include <map>
#include <string>

enum Action {
    // ...
};

enum Color {// 🂠 - blank card
    SPADE,  // ♠ - black
    CLUB,   // ♣ - black
    HEART,  // ♥ - red
    DIAMOND // ♦ - red
};

const std::map<Color, std::string> COLOR_STRING_VALUES = {
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

const std::map<Value, std::string> VALUE_STRING_VALUES = {
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
    int cardAmount;
    boost::container::list<Card> cards;

    Player() {
        maxCards = 0;
        cardAmount = 0;
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