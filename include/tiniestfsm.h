#pragma once

#include <tuple>
#include <type_traits>
#include <algorithm>
#include <array>

#define _TINIEST_FSM_DISPATCH_4(n, X) \
    X(n);                 \
    X(n + 1);             \
    X(n + 2);             \
    X(n + 3)
#define _TINIEST_FSM_DISPATCH_16(n, X) \
    _TINIEST_FSM_DISPATCH_4(n, X);     \
    _TINIEST_FSM_DISPATCH_4(n + 4, X); \
    _TINIEST_FSM_DISPATCH_4(n + 8, X); \
    _TINIEST_FSM_DISPATCH_4(n + 12, X)
#define _TINIEST_FSM_DISPATCH_64(n, X)   \
    _TINIEST_FSM_DISPATCH_16(n, X);      \
    _TINIEST_FSM_DISPATCH_16(n + 16, X); \
    _TINIEST_FSM_DISPATCH_16(n + 32, X); \
    _TINIEST_FSM_DISPATCH_16(n + 48, X)
#define _TINIEST_FSM_DISPATCH_256(n, X)   \
    _TINIEST_FSM_DISPATCH_64(n, X);       \
    _TINIEST_FSM_DISPATCH_64(n + 64, X);  \
    _TINIEST_FSM_DISPATCH_64(n + 128, X); \
    _TINIEST_FSM_DISPATCH_64(n + 192, X)

#define _TINIEST_FSM_DISPATCH(n, x) x(_TINIEST_FSM_DISPATCH##_##n, n)

#if defined(__clang__) || defined(__GNUC__)
#define _TINIEST_FSM_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define _TINIEST_FSM_UNREACHABLE __assume(false)
#else // unknown compiler
#define _TINIEST_FSM_UNREACHABLE
#endif

#define _TINIEST_FSM_CASE(n)                    \
    case (n):                                   \
        if constexpr ((n) < s_nStates) {        \
            processFromIndex<(n)>(ev);          \
            break;                              \
        }                                       \
        _TINIEST_FSM_UNREACHABLE

#define _TINIEST_FSM_DISPATCH_IMPL(dispatcher, n)       \
    switch (m_currentState) {                           \
        dispatcher(0, _TINIEST_FSM_CASE);               \
    default:                                            \
        _TINIEST_FSM_UNREACHABLE;                       \
    }

namespace tiniest_fsm {

    namespace details {

        // returns true if type Elem is one of the types in List
        template <typename Elem, typename...List>
        inline constexpr bool elem_in_list_v = (false || ... || std::is_same_v<Elem, List>);

        // tests for elem_in_list_v
        static_assert(elem_in_list_v<int, int, bool>);
        static_assert(elem_in_list_v<int, int>);
        static_assert(elem_in_list_v<int, bool, int>);
        static_assert(!elem_in_list_v<double, int, bool>);

        // returns the number of occurrences of Elem in List
        template <typename Elem, typename...List>
        inline constexpr unsigned count_in_list_v = (0u + ... + std::is_same_v<Elem, List>);

        // returns true if the list of types Ts contains no duplicates 
        template <typename...Ts>
        inline constexpr bool are_distinct_v = (true && ... && (count_in_list_v<Ts, Ts...> == 1));

        // tests for are_distinct_v
        static_assert(!are_distinct_v<int, int, bool>);
        static_assert(!are_distinct_v<int, bool, int>);
        static_assert(are_distinct_v<int>);
        static_assert(are_distinct_v<int, bool, double>);

    } // namespace details

    template <typename...Ts>
    class StateMachine;

    template <typename DerivedClass, typename...States>
        requires
            ( details::are_distinct_v<States...>   // states must be distinct types
            && std::conjunction_v<std::is_class<States>...>    // states must be classes
            && (sizeof...(States) > 0)
            )
    // The first argument is the class itself, the second argument is a tuple with all possible states
    // The tuple of states is intantiated as a data member of the base class.
    // States types must be unique and must be classes
    class StateMachine<DerivedClass, std::tuple<States...>>
    {
        // *****************************
        // constants
        //

        static constexpr size_t s_nStates = sizeof...(States);

        // *****************************
        // typedefs
        //


        using this_t = StateMachine<DerivedClass, std::tuple<States...>>;
        using FSM = DerivedClass;

        // *****************************
        // data members
        //

        unsigned m_currentState;
        [[no_unique_address]] std::tuple<States...> m_states;

        // *****************************
        // auxiliary functions
        //

        template <typename State>
        static constexpr bool valid_state_v = details::elem_in_list_v<State, States...>;

        template <typename State, typename Event>
        static constexpr bool has_handler_v = requires (State && s, const Event & ev) { s.handle((FSM*)nullptr, ev); };

        FSM* fsm() { return (FSM*)this; }

        template <typename State>
        static consteval size_t getStateIndex()
        {
            static_assert(valid_state_v<State>, "invalid state type");
            constexpr std::array<bool, s_nStates> isIt = { std::is_same_v<State,States>... };
            return std::distance(isIt.begin(), std::ranges::find(isIt, true));
        }

        template <typename State, typename Event>
        void processFromState(const Event& ev)
        {
            if constexpr (has_handler_v<State, Event>)
                std::get<State>(m_states).handle(fsm(), ev);
        }

        template <uint16_t StateIndex, typename Event>
        void processFromIndex(const Event& ev)
        {
            static_assert(StateIndex < sizeof...(States), "invalid state index");
            using State = std::tuple_element_t<StateIndex, std::tuple<States...>>;
            processFromState<State>(ev);
        }

    public:

        unsigned currentStateId() const
        {
            return m_currentState;
        }

        template <typename State>
        State& getState()
        {
            static_assert(valid_state_v<State>, "invalid state type");
            return std::get<State>(m_states);
        }

        template <typename State>
        const State& getState() const
        {
            static_assert(valid_state_v<State>, "invalid state type");
            return std::get<State>(m_states);
        }

        // In sequence, this function does:
        //   - change state to NewState
        //   - calls NewState::enter  (if it exists)
        template <typename NewState>
        void enterState()
        {
            static_assert(valid_state_v<NewState>, "invalid state type");

            // change state
            m_currentState = getStateIndex<NewState>();

            // if there is a method NewState::enter(FSM*), then invoke it
            constexpr bool hasEnter = requires (NewState && s) { s.enter(fsm()); };
            if constexpr (hasEnter)
                std::get<NewState>(m_states).enter(fsm());
        }

        template <typename Event>
        void process(const Event& ev)
        {
            // to debug this, just put a breakpoint in the function _process
            if constexpr (s_nStates <= 4) {
                _TINIEST_FSM_DISPATCH(4, _TINIEST_FSM_DISPATCH_IMPL)
            }
            else if constexpr (s_nStates <= 16) {
                _TINIEST_FSM_DISPATCH(16, _TINIEST_FSM_DISPATCH_IMPL)
            }
            else if constexpr (s_nStates <= 64) {
                _TINIEST_FSM_DISPATCH(64, _TINIEST_FSM_DISPATCH_IMPL)
            }
            else if constexpr (s_nStates <= 256) {
                _TINIEST_FSM_DISPATCH(256, _TINIEST_FSM_DISPATCH_IMPL)
            }
            else {
                using process_fun_t = void (StateMachine::*)(const Event&);
                static constexpr process_fun_t f[s_nStates] = { (has_handler_v<States,Event> ? &this_t::processFromState<States, Event> : nullptr)... };
                if (f[m_currentState])
                    std::invoke(f[m_currentState], this, ev);
            }
        }
    };

} // namespace tiniest_fsm

#undef _TINIEST_FSM_DISPATCH_4
#undef _TINIEST_FSM_DISPATCH_16
#undef _TINIEST_FSM_DISPATCH_64
#undef _TINIEST_FSM_DISPATCH_256
#undef _TINIEST_FSM_DISPATCH
#undef _TINIEST_FSM_DISPATCH_IMPL
#undef _TINIEST_FSM_CASE
#undef _TINIEST_FSM_UNREACHABLE


/*

#include <type_traits>
#include <array>
#include <algorithm>
#include <numeric>
#include <ranges>
//#include <iostream>
using namespace std;

template <typename...Ts> struct A;

template <size_t I>
inline constexpr auto good_v = I % 3 == 0;

template <size_t I>
inline constexpr auto res_v = I * 5;

template <size_t...Used, size_t Head>
struct A<index_sequence<Used...>, index_sequence<Head>>
{
    using Used2 = conditional_t<good_v<Head>,
        index_sequence<Used..., Head>, index_sequence<Used...>>;
    using type = Used2;
};

template <size_t...Used, size_t Head, size_t...Tail>
struct A<index_sequence<Used...>, index_sequence<Head,Tail...>>
{
    using Used2 = conditional_t<good_v<Head>,
        index_sequence<Used..., Head>, index_sequence<Used...>>;
    using Tail2 = index_sequence<Tail...>;
    using type = typename A<Used2, Tail2>::type;
};

template <size_t...Is>
constexpr auto mkarray(index_sequence<Is...>&&)
{
    using p = std::pair<size_t, decltype(res_v<0>)>;
    return std::array<p, sizeof...(Is)>{make_pair(Is, res_v<Is>)...};
}

template <size_t N>
constexpr inline auto even()
{
    using seq = make_index_sequence<N>;
    using ev = A<index_sequence<>,seq>::type;

   return mkarray(ev{});
}


auto foo(int n)
{
    static constexpr auto z = even<100>();
    return z[n];
}


*/