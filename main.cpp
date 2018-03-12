/*
 * Copyright 2018 Jeremy William Murphy
 * email: jeremy.william.murphy@gmail.com
 *
 * License coming...
 */

#include <boost/functional/factory.hpp>
#include <boost/function.hpp>
#include <boost/variant.hpp>

#include <map>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
// Just for debugging.
#include <iostream>
#include <typeinfo>
#include <cxxabi.h>


template <typename Signature>
struct signature_impl;

template <typename ReturnType, typename... Args>
struct signature_impl<ReturnType(Args...)>
{
    using return_type = ReturnType;
    using param_types = std::tuple<Args...>;
};

template <typename T>
using signature_t = signature_impl<T>;


// Tuple manipulation replaced by std::apply in C++17.

template <std::size_t... Ints>
struct indices {};

template <std::size_t N, std::size_t... Ints>
struct build_indices : build_indices<N-1, N-1, Ints...> {};

template <std::size_t... Ints>
struct build_indices<0, Ints...> : indices<Ints...> {};

template <typename Tuple>
using make_tuple_indices = build_indices<std::tuple_size<typename std::remove_reference<Tuple>::type>::value>;


template <class AbstractProduct, typename IdentifierType, typename... ProductCreators>
class multifactory
{
    using functions = boost::variant<boost::function<ProductCreators>...>;

    std::map<IdentifierType, functions> associations_;

    template <typename Signature>
    struct dispatch_foo
    {
        template <typename CreateArgs, std::size_t... Indices>
        typename std::enable_if<std::is_convertible<CreateArgs, typename signature_t<Signature>::param_types>::value, AbstractProduct>::type
        static apply(boost::function<Signature> const &f, CreateArgs && t, indices<Indices...>)
        {
            return f(std::get<Indices>(std::forward<CreateArgs>(t))...);
        }

        template <typename CreateArgs, std::size_t... Indices>
        typename std::enable_if<!std::is_convertible<CreateArgs, typename signature_t<Signature>::param_types>::value, AbstractProduct>::type
        static apply(boost::function<Signature> const &, CreateArgs &&, indices<Indices...>)
        {
            return nullptr;
        }
    };

    template <typename... CreateArguments>
    struct dispatcher : boost::static_visitor<AbstractProduct>
    {
        std::tuple<CreateArguments...> args;

        dispatcher(CreateArguments const&... args) : args{std::forward_as_tuple(args...)} {}

        template <typename Signature>
        AbstractProduct operator()(boost::function<Signature> const &f) const
        {
            int status;
            std::cout << "visitor: " << abi::__cxa_demangle(typeid(Signature).name(), nullptr, 0, &status) << "\n";
            return dispatch_foo<Signature>::apply(f, args, make_tuple_indices<std::tuple<CreateArguments...>>{});
        }
    };

public:
    template <typename ProductCreator>
    bool Register(IdentifierType id, ProductCreator &&creator) {
        return associations_.emplace(id, std::forward<ProductCreator>(creator)).second;
    }

    bool Unregister(const IdentifierType& id) {
        return associations_.erase(id) == 1;
    }

    template <typename... Arguments>
    AbstractProduct CreateObject(const IdentifierType& id, Arguments const& ... args) {
        auto i = associations_.find(id);
        if (i != associations_.end()) {
            dispatcher<Arguments...> impl(args...);
            return boost::apply_visitor(impl, i->second);
        }
        throw std::runtime_error("Creator not found.");
    }
};

struct Arity {
    virtual ~Arity() = default;
};

struct Nullary : Arity {};

template <typename T>
struct Unary : Arity {
    Unary() {} // Also has nullary ctor.
    Unary(T) {}
};

struct Binary : Arity {
    Binary() {} // Also has nullary ctor.
    Binary(int, int) {}
};


int main(void)
{
    multifactory<Arity*, int, Arity*(), Arity*(int const &), Arity*(int const &, int const&), Arity*(double const&)> factory;
    factory.Register(0, boost::function<Arity*()>( boost::factory<Nullary*>() ));
    factory.Register(1, boost::function<Arity*(int const &)>(boost::factory<Unary<int>*>()) );
    factory.Register(2, boost::function<Arity*(int const &, int const &)>(boost::factory<Binary*>()) );
    factory.Register(3, boost::function<Arity*(double const &)>(boost::factory<Unary<double>*>()) );
    auto a = factory.CreateObject(0);
    assert(a);
    assert(typeid(*a) == typeid(Nullary));
    auto b = factory.CreateObject(1, 2);
    assert(b);
    assert(typeid(*b) == typeid(Unary<int>));
    auto c = factory.CreateObject(2, 9, 2, 3, 4);
    assert(!c);
}
