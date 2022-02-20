#include <tiniestfsm.h>

#include <iostream>

struct OpenEvent {};
struct CloseEvent {};
struct LockEvent { unsigned key; };
struct UnlockEvent { unsigned key; };

// some forward declaration
struct CloseState;
struct LockedState; 

struct OpenState
{
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

    static void handle(auto* door, const LockEvent& ev)
    {
        std::cout << "CloseState handling LockEvent\n";
        if (ev.key == door->m_key)
            door->template enterState<LockedState>();
    }
};


struct LockedState
{
    static void handle(auto* door, const UnlockEvent& ev)
    {
        std::cout << "LockedState handling UnlockEvent\n";
        if (ev.key == door->m_key)
            door->template enterState<CloseState>();
    }
    int x;
};

struct MyFSM : public tiniest_fsm::StateMachine<MyFSM, std::tuple<OpenState, CloseState, LockedState>>
{
    MyFSM(unsigned key) : m_key(key) {}
    unsigned m_key;
};

int main()
{
    MyFSM fsm{123u};
    fsm.enterState<OpenState>();
    fsm.process(CloseEvent{});
    fsm.process(OpenEvent{});
    fsm.process(CloseEvent{});
    fsm.process(LockEvent{ 521u });    // wrong key, nothing happens
    fsm.process(LockEvent{ 123u });
    fsm.process(UnlockEvent{ 521u });  // wrong key, nothing happens
    fsm.process(UnlockEvent{ 123u });
    return 0;
}