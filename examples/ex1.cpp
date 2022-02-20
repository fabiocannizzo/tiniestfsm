#include <tiniestfsm.h>

#include <iostream>

struct OpenEvent {};
struct CloseEvent {};

// some forward declaration
struct CloseState;

struct OpenState
{
    // We specify 'door' as a template class, because the 'Door' class is not defined yet
    static void handle(auto* door, const CloseEvent& ev)
    {
        std::cout << "OpenState handling CloseEvent\n";
        door->template enterState<CloseState>();
    }
};

struct CloseState
{
    static void handle(auto* door, const OpenEvent& ev)
    {
        std::cout << "CloseState handling OpenEvent\n";
        door->template enterState<OpenState>();
    }
};

struct Door : public tiniest_fsm::StateMachine<Door, std::tuple<OpenState, CloseState>>
{
};

int main()
{
    Door door{};
    door.enterState<OpenState>();
    door.process(CloseEvent{});
    door.process(CloseEvent{});  // the door is already closed, nothing happens
    door.process(OpenEvent{});
    return 0;
}