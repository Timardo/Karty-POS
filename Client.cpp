#include <boost/asio.hpp>
#include "color.hpp"
#include "shared.h"
#include <conio.h>

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::cout;
using std::endl;

auto Card::toString() {
        return color <= CLUB ? dye::white_on_black(VALUE_STRING_VALUES.at(value) + COLOR_STRING_VALUES.at(color)) : dye::red_on_black(VALUE_STRING_VALUES.at(value) + COLOR_STRING_VALUES.at(color));
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    Card card = Card(DIAMOND, SEVEN);
    int char_ = getch();
    cout << card.toString() << endl;
    system("cls");
    cout << card.toString() << endl;
    ClientData clientData;
    clientData.lastCard = Card(CLUB, SEVEN);
    clientData.deckCards = 24;
    Player otherPlayer = Player();
    otherPlayer.cards.push_back(Card(CLUB, KING));
    clientData.otherPlayers.push_back(otherPlayer);
    clientData.clientPlayer = Player();
    clientData.clientPlayer.cards.push_back(Card(CLUB, NINE));

    io_service io_service;
    tcp::socket socket(io_service); //socket creation
    socket.connect(tcp::endpoint(ip::address::from_string("[NOT_AVAILABLE]"), 10234)); //connection

    boost::system::error_code error;
    Packet startGamePacket = Packet(StartGame);
    cout << "press any key to start game, press k to not start" << endl;
    char c = getch();

    while (c != 'k') {
        writeToSocket(socket, startGamePacket.toString());
        c = getch();
    }

    return 0;
}
