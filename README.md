# The Tiniest Finite State Machine Ever
This is a header-only minimalist implementation of a finite state machine.

It makes extensive use of templates and requires C++20. It does not use RTTI.

It is designed to achieved the following goals:

* implementation simplicity

* high performance

* minimal memory footprint

* suitable for embedded system

# Example 1 `(ex1.cpp)`

Let's consider a door, which can be in the two possible states *OPEN* and *CLOSED*.
The possible events are *open* and *close*.
The state transition diagram is shown here below.

![Alt text](https://g.gravizo.com/svg?
  digraph G {
  rankdir=LR
  start -> OPEN [label=start];
  OPEN -> CLOSE [label=close];
  CLOSE -> OPEN [label=open];
  start [shape=point];
  }
)

First we implement the events:

```c++

    struct OpenEvent {};
    struct CloseEvent {};
```

Then we implement the states and for each state we define a handler for the events that the state should handle.
Note that we use a template argument `door` for the state machine, becuase the `Door` class is not defined yet.

```c++

    struct OpenState
    {
        void handle(auto* door, const CloseEvent&) {
            // we simply transtion from OpenState to CloseState
            door->template enterState<CloseState>();
        }
    };

    struct CloseState
    {
        void handle(auto* door, const OpenEvent&) {
            // we simply transtion from CloseState to OpenState
            door->template enterState<OpenState>();
        }
    };
```

Last, we define the finite state machine:

```c++

    // StateMachine requires a list of template arguments. 
    // The first argument is the class itself, the second argument is a tuple with all possible states.
    // The tuple of states is intantiated as a data member of the base class.
    // States types must be unique and must be classes.
    struct Door : public tiniest_fsm::StateMachine<Door, std::tuple<OpenState, CloseState>>
    {
    };
```

That is it, really! We are ready to use the finite state machine we just defined.

```c++

int main()
{
    Door door{};

    // we initialize the door to be in state OpenState
    door.enterState<OpenState>();

    // we process a few events
    door.process(CloseEvent{});
    door.process(OpenEvent{});

    return 0;
}
```

# Example 2 `(ex2.cpp)`

Let's complicate a little bit the previous examples. The door can now be in the three possible states *OPEN*, *CLOSED* or *LOCKED*.
The possible events are *open*, *close*, *lock* and *unlock*.
To lock or unlock the door the right key is required and the operation can fail if the key is not valid.
The state transition diagram is shown here below.

![Alt text](https://g.gravizo.com/svg?
  digraph G {
  rankdir=LR
  start -> OPEN [label=start];
  OPEN -> CLOSE [label=close];
  CLOSE -> OPEN [label=open];
  CLOSE ->  LOCKED  [label="lock (ok)"];
  CLOSE ->  CLOSE [label="lock (fail)"];
  LOCKED ->  CLOSE [label="unlock (ok)"];
  LOCKED ->  LOCKED [label="unlock (fail)"];
  start [shape=point];
    }
)

We need two additional events:

```c++

    struct LockEvent { unsigned key; };
    struct UnlockEvent { unsigned key; };
```

Note that thee `LockEvent` and `UnlockEvent` have a `key` member variable.

We also need a new state `LockedState`:

```c++

    struct LockedState
    {
        static void handle(auto* door, const UnlockEvent& ev)
        {
            if (ev.key == door->m_key)
                door->template enterState<CloseState>();
        }
    };
```

We modify `CloseState` to handle also `LockEvent`:

```c++

    struct CloseState
    {
        static void handle(auto* door, const LockEvent& ev)
        {
            std::cout << "CloseState handling LockEvent\n";
            if (ev.key == door->m_key)
                door->template enterState<LockedState>();
        }

        // ... other handlers
    };
```

Last we need to update the `Door` class addin the new state `LockedState` and the `key` data member:


```c++

    struct Door : public tiniest_fsm::StateMachine<Door, std::tuple<OpenState, CloseState, LockedState>>
    {
        Door(unsigned key) : m_key(key) {}
        unsigned m_key;
    };
```

and we can now close and open the door.

```c++

int main()
{
    Door door{123u};

    // we initialize the door to be in state OpenState
    door.enterState<OpenState>();

    // handle same events
    door.process(CloseEvent{});
    door.process(OpenEvent{});
    door.process(CloseEvent{});
    door.process(LockEvent{ 521u });    // wrong key, nothing happens
    door.process(LockEvent{ 123u });
    door.process(UnlockEvent{ 521u });  // wrong key, nothing happens
    door.process(UnlockEvent{ 123u });

    return 0;
}
```

# Minimal Footprint

Given the `Door` class defined in example 2 (`ex2.cpp`), the function

```c++

    void foo(Door *door)
    {
        door->process(OpenEvent{});
    }
```

is translated by gcc to

```asm

    foo(Door&):
            ; gcc can see that only ClosedState has a handler for OpenEvent
            ; therefore it just checks if the current state index is ClosedState's one (i.e. 1, in this case)
            cmp     DWORD PTR [rdi], 1
            jne     .L1
            ; If we get here, we are in the ClosedEvent state, so we runs the handler.
            ; Note that the handler is fully inlined. It simply changes the current 
            ; state index to OpenEvent (i.e. 0, in this case)
            mov     DWORD PTR [rdi], 0
    .L1:
            ret
```

How tinier could it possibly be?

# The StateMachine class

The `StateMachine` class is defined as:

```c++

    template <typename...Ts>
    class StateMachine;

    template <typename DerivedClass, typename...States>
    class StateMachine<DerivedClass, std::tuple<States...>> { /* implementation */ };
```

It requires two template arguments:

- `DerivedClass` is the concrete class which inherits from `StateMachine`. 

- `std::tuple<States...>` is a tuple with all possible states. States must satisfy the `is_class_v<S>` predicate and must be unique.

The `StateMachine` declares internally a data member variable of type `std::tuple<States...>`.

# API

The `StateMachine` class exposes the following `public` methods:

```c++

    // Returns the index of the current state
    // The index correspond to the ordinal position of the current state in tuple of states
    unsigned currentStateId() const;
    
    // Enters the state NewState
    template <typename NewState>
    void enterState();

    // Processed an event, if the handler for this event is defined in the current state
    template <typename Event>
    void process(const Event& ev);

    // Return a modifiable reference to the state
    template <typename State>
    State& getState();

    // Return a non-modifiable reference to the state
    template <typename State>
    const State& getState() const;
```

# State Classes

The only requirement for state classes is they must implement handlers for relevant events.

Handlers can be static or non-static members. They must have header:

```c++

    void handle(FSM *, const Event&);
```

Optionally, it is possible to define in state classes the methods `enter`.
If defined, this is automatically called by the function `StateMachine<...>::enterState`
every time there is a state transition. It must have header:

```c++

    void enter(FSM *);
```

