// Useful type traits used mostly internally by Parlay
//
// Many inspired by this video, and the following standards
// proposals:
//  - https://www.youtube.com/watch?v=MWBfmmg8-Yo
//  - http://open-std.org/JTC1/SC22/WG21/docs/papers/2014/n4034.pdf
//  - https://quuxplusone.github.io/blog/code/object-relocation-in-terms-of-move-plus-destroy-draft-7.html
//
// Includes:
//  - priority_tag
//  - is_contiguous_iterator / is_random_access_iterator
//  - is_trivial_allocator
//  - is_trivially_relocatable / is_nothrow_relocatable
//

#ifndef PARLAY_TYPE_TRAITS_H
#define PARLAY_TYPE_TRAITS_H

#include <cstddef>

#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace parlay {

/*  --------------------- Priority tags. -------------------------
    Priority tags are an easy way to force template resolution to
    pick the "best" option in the presence of multiple valid
    choices. It works because of the facts that priority_tag<K>
    is a subtype of priority_tag<K-1>, and template resolution
    will always pick the most specialised option when faced with
    a choice, so it will prefer priority_tag<K> over
    priority_tag<K-1>
*/

template<size_t K>
struct priority_tag : priority_tag<K-1> {};

template<>
struct priority_tag<0> {};

/*  --------------------- Contiguous iterators -----------------------
    An iterator is a contiguous iterator if it points to memory that
    is contiguous. More specifically, it means that given an iterator
    a, and an index n such that a+n is a valid, dereferencable iterator,
    then *(a + n) is equivalent to *(std::addressof(*a) + n).

    C++20 will introduce a concept for detecting contiguous iterators.
    Until then, we just do the conservative thing and deduce that any
    iterators represented as pointers are contiguous.

    We also supply a convenient trait for checking whether an iterator
    is a random access iterator. This can be done using current C++,
    but is too verbose to type frequently, so we shorten it here.
*/

template<typename It>
struct is_contiguous_iterator : std::is_pointer<It> {};

template<typename It>
inline constexpr bool is_contiguous_iterator_v = is_contiguous_iterator<It>::value;

template<typename It>
struct is_random_access_iterator : std::bool_constant<
    std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<It>::iterator_category>> {};

template<typename It>
inline constexpr bool is_random_access_iterator_v = is_random_access_iterator<It>::value;

/*  ----------------- Trivial allocators. ---------------------
    Allocator-aware containers and algorithms need to know whether
    they can construct/destruct objects directly inside memory given
    to them by an allocator, or whether the allocator has custom
    behaviour. Since some optimizations require us to circumvent
    custom allocator behaviour, we need to detect when an allocator
    does not do this.

    Specifically, an allocator-aware algorithm must construct objects
    inside memory returned by an allocator by writing

    std::allocator_traits<allocator_type>::construct(allocator, p, args);

    if the allocator type defines a method .construct, then this results
    in forwarding the construction to that method. Otherwise, this just
    results in a call to

    new (p) T(std::forward<Args>(args)...)

    If we wish to circumvent calling the constructor, for example,
    for a trivially relocatable type in which we would prefer to
    copy directly via memcpy, we must ensure that the allocator
    does not have a custom .construct method. Otherwise, we can
    not optimize, and must continue to use the allocator's own
    construct method.

    The same discussion is true for destruction as well.

    See https://www.youtube.com/watch?v=MWBfmmg8-Yo for more info.
*/

namespace internal {

  // Detect the existence of the .destroy method of the type Alloc
  template<typename Alloc, typename T>
  auto trivial_allocator(Alloc& a, T *p, priority_tag<2>)
    -> decltype(void(a.destroy(p)), std::false_type());

  // Detect the existence of the .construct method of the type Alloc
  template<typename Alloc, typename T>
  auto trivial_allocator(Alloc& a, T *p, priority_tag<1>)
    -> decltype(void(a.construct(p, std::declval<T&&>())), std::false_type());

  // By default, if no .construct or .destroy methods are found, assume
  // that the allocator is trivial
  template<typename Alloc, typename T>
  auto trivial_allocator(Alloc& a, T* p, priority_tag<0>)
    -> std::true_type;

}  // namespace internal

template<typename Alloc, typename T>
struct is_trivial_allocator
    : decltype(internal::trivial_allocator<Alloc, T>(std::declval<Alloc&>(), nullptr, priority_tag<2>())) {};

template<typename Alloc, typename T>
inline constexpr bool is_trivial_allocator_v = is_trivial_allocator<Alloc, T>::value;

// Manually specialize std::allocator since it is trivial, but
// some (maybe all?) implementations still provide a .construct
// and .destroy method anyway.
template<typename T>
struct is_trivial_allocator<std::allocator<T>, T> : std::true_type {};

/*  ----------------- Trivially relocatable. ---------------------
    A type T is called trivially relocatable if, given a pointer
    p to an object of type T, and a pointer q to unintialized
    memory large enough for an object of type T, then

    new (q) T(std::move(*p));
    p->~T();

    is equivalent to

    std::memcpy(p, q, sizeof(T));

    Any type that is trivially move constructible and trivially
    destructible is therefore trivially relocatable. User-defined
    types that are not obviously trivially relocatable can be
    annotated as such by specializing the is_trivially_relocatable
    type.

    See proposal D1144R0 for copious details:
    https://quuxplusone.github.io/blog/code/object-relocation-in-terms-of-move-plus-destroy-draft-7.html
*/

template <typename T>
struct is_trivially_relocatable :
        std::bool_constant<std::is_trivially_move_constructible<T>::value &&
                           std::is_trivially_destructible<T>::value> { };

template <typename T> struct is_nothrow_relocatable :
        std::bool_constant<is_trivially_relocatable<T>::value ||
                           (std::is_nothrow_move_constructible<T>::value &&
                            std::is_nothrow_destructible<T>::value)> { };

template<typename T>
inline constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

template<typename T>
inline constexpr bool is_nothrow_relocatable_v = is_nothrow_relocatable<T>::value;

// The standard allocator is stateless, so it is trivially relocatable,
// but unfortunately it is not detected as such, so we mark it manually.
// This is important because parlay::sequence<T, Alloc> is only trivially
// relocatable when its allocator is trivially relocatable.

template<typename T>
struct is_trivially_relocatable<std::allocator<T>> : std::true_type {};

template <typename T1, typename T2>
struct is_trivially_relocatable<std::pair<T1,T2>> : 
    std::bool_constant<is_trivially_relocatable<T1>::value &&
		       is_trivially_relocatable<T2>::value> {};

}  // namespace parlay

#endif //PARLAY_TYPE_TRAITS_H
