#ifndef INTRINSICS_H
#define INTRINSICS_H

#include "u8port.h"

#include <type_traits>
#include <utility>
#include "farptr.h"

// Usecode's intrinsics: the C++ functions usecode calls by number (opcode 0Fh).
//
// On DOS the interpreter copied the call's argument bytes from the usecode stack onto the
// CPU stack and called the function's far address, so each function read its arguments as
// Borland passed them: a member function's far this first, then the parameters in order;
// int, unsigned, char and enums as 2 bytes, long and pointers and references as 4. The
// result came back in DX:AX. Here each intrinsic has a small wrapper that reads its
// arguments the same way and calls the function; the wrappers come from the function's own
// declaration (IntrinsicCall below), so nothing is written out by hand.

struct IntrinsicEntry
{
	uint16_t segment;	// its address in U8.EXE's link map, as unkcoff.dat names it
	uint16_t offset;
	const char *name;	// as the map gave it
	uint32_t (*call)(const char *args);
};

extern const IntrinsicEntry intrinsics[];
extern const uint16_t numIntrinsics;
// The entry at this map address; one that halts with the address when there is none.
const IntrinsicEntry *findIntrinsic(uint16_t segment, uint16_t offset);

namespace intrinsic_detail
{
	// How many bytes Borland pushed for an argument of type T.
	template <class T> constexpr int16_t size()
	{
		if constexpr (std::is_pointer_v<T> || std::is_reference_v<T>)
			return 4;
		else if constexpr (sizeof(T) == 4)
			return 4;
		else
			return 2;
	}

	template <class T> T read(const char *at)
	{
		if constexpr (std::is_reference_v<T>)
			return *(std::remove_reference_t<T> *)u8_ptr_from_far(u8_far_load(at));
		else if constexpr (std::is_pointer_v<T>)
			return (T)u8_ptr_from_far(u8_far_load(at));
		else if constexpr (std::is_enum_v<T>)
		{
			int16_t v;
			memcpy(&v, at, 2);
			return (T)v;
		}
		else if constexpr (sizeof(T) == 4)
		{
			T v;
			memcpy(&v, at, 4);
			return v;
		}
		else if constexpr (sizeof(T) == 2)
		{
			T v;
			memcpy(&v, at, 2);
			return v;
		}
		else
			return (T)at[0];	// a char was pushed as a word; its low byte
	}

	// The offset of each parameter, the this pointer (if any) taking the first 4 bytes.
	template <class... P, size_t... I>
	constexpr auto offsets(int16_t start, std::index_sequence<I...>)
	{
		constexpr int16_t sizes[] = {size<P>()..., 0};
		struct Offsets { int16_t at[sizeof...(P) + 1]; } result = {};
		int16_t at = start;
		for (size_t i = 0; i < sizeof...(P); i++)
		{
			result.at[i] = at;
			at += sizes[i];
		}
		return result;
	}

	// The result as DX:AX held it.
	template <class R> uint32_t result(R r)
	{
		if constexpr (std::is_pointer_v<R>)
			return u8_far_from_ptr((const void *)r);
		else if constexpr (std::is_enum_v<R> || std::is_signed_v<R>)
			return (uint32_t)(int32_t)r;
		else
			return (uint32_t)r;
	}

	template <class R, class C, class... P, size_t... I>
	uint32_t callMember(R (C::*f)(P...), const char *args, std::index_sequence<I...> seq)
	{
		constexpr auto at = offsets<P...>(4, seq);
		C *self = (C *)u8_ptr_from_far(u8_far_load(args));
		if constexpr (std::is_void_v<R>)
		{
			(self->*f)(read<P>(args + at.at[I])...);
			return 0;
		}
		else
			return result<R>((self->*f)(read<P>(args + at.at[I])...));
	}

	template <class R, class... P, size_t... I>
	uint32_t callFree(R (*f)(P...), const char *args, std::index_sequence<I...> seq)
	{
		constexpr auto at = offsets<P...>(0, seq);
		if constexpr (std::is_void_v<R>)
		{
			f(read<P>(args + at.at[I])...);
			return 0;
		}
		else
			return result<R>(f(read<P>(args + at.at[I])...));
	}

	template <class R, class C, class... P>
	uint32_t call(R (C::*f)(P...), const char *args)
	{
		return callMember(f, args, std::index_sequence_for<P...>());
	}

	template <class R, class... P>
	uint32_t call(R (*f)(P...), const char *args)
	{
		return callFree(f, args, std::index_sequence_for<P...>());
	}

	// Picks the overload with these parameter types, member or not, whatever it returns.
	template <class... P> struct Pick
	{
		template <class R, class C> static constexpr auto get(R (C::*f)(P...)) { return f; }
		template <class R> static constexpr auto get(R (*f)(P...)) { return f; }
	};
}

// The wrapper for the function f with parameter types P...
#define INTRINSIC(f, ...) \
	[](const char *args) -> uint32_t { \
		return intrinsic_detail::call(intrinsic_detail::Pick<__VA_ARGS__>::get(&f), args); }

#endif
