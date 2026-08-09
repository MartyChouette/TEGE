#pragma once

// Calling-convention shim for AngelScript bindings.
//
// AngelScript has no native calling-convention support on WASM: as_config.h
// defines AS_MAX_PORTABILITY under Emscripten and every RegisterGlobalFunction/
// RegisterObjectMethod/RegisterObjectBehaviour call using asCALL_CDECL /
// asCALL_THISCALL / asCALL_CDECL_OBJFIRST / asCALL_CDECL_OBJLAST fails with
// asNOT_SUPPORTED (-7). Before this shim the web player registered ZERO
// bindings and every script failed to compile ("Invalid configuration").
//
// All binding registrations must therefore go through these macros:
//
//   engine->RegisterGlobalFunction("void Foo(int)",
//       ENJIN_AS_FN(Script_Foo), ENJIN_AS_CALL_CDECL);
//   engine->RegisterObjectMethod("Entity", "bool IsValid()",
//       ENJIN_AS_MFN(EntityHandle, IsValid), ENJIN_AS_CALL_THISCALL);
//   engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f()",
//       ENJIN_AS_OBJ_LAST(Vector2_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST);
//
// On desktop these expand to the exact native forms (asFUNCTION + asCALL_CDECL
// etc.) — zero behavior change. Under AS_MAX_PORTABILITY they expand to
// variadic generic thunks (same Proxy technique as the official autowrapper
// add-on, which only ships with fixed 4-arg arity) and asCALL_GENERIC.
//
// Overloaded functions cannot be wrapped by name — if you ever need
// asFUNCTIONPR/asMETHODPR, extend this header with _PR variants first.

#include <angelscript.h>

#ifndef AS_MAX_PORTABILITY

#define ENJIN_AS_FN(f)                 asFUNCTION(f)
#define ENJIN_AS_OBJ_FIRST(f)          asFUNCTION(f)
#define ENJIN_AS_OBJ_LAST(f)           asFUNCTION(f)
#define ENJIN_AS_MFN(c, m)             asMETHOD(c, m)
#define ENJIN_AS_CALL_CDECL            asCALL_CDECL
#define ENJIN_AS_CALL_CDECL_OBJFIRST   asCALL_CDECL_OBJFIRST
#define ENJIN_AS_CALL_CDECL_OBJLAST    asCALL_CDECL_OBJLAST
#define ENJIN_AS_CALL_THISCALL         asCALL_THISCALL

#else // AS_MAX_PORTABILITY: wrap everything as asCALL_GENERIC thunks

#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Enjin::Scripting::GenWrap {

// Official-autowrapper Proxy technique: an AngelScript generic arg slot holds
// the value (primitives) or a pointer (objects / references). Reinterpreting
// the slot as Proxy<T> reads it back as the exact C++ parameter type,
// including reference types (represented as pointers).
template <typename T>
struct Proxy {
    T value;
    static T cast(void* ptr) {
        return reinterpret_cast<Proxy<T>*>(&ptr)->value;
    }
};

template <typename T>
T ArgValue(asIScriptGeneric* gen, asUINT i) {
    return static_cast<Proxy<T>*>(gen->GetAddressOfArg(i))->value;
}

// --- free function: R (*)(A...) -------------------------------------------
template <typename Sig> struct FreeFn;

template <typename R, typename... A>
struct FreeFn<R (*)(A...)> {
    template <R (*fp)(A...), size_t... Is>
    static void Call(asIScriptGeneric* gen, std::index_sequence<Is...>) {
        if constexpr (std::is_void_v<R>) {
            fp(ArgValue<A>(gen, static_cast<asUINT>(Is))...);
        } else {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>{ fp(ArgValue<A>(gen, static_cast<asUINT>(Is))...) };
        }
    }
    template <R (*fp)(A...)>
    static void Thunk(asIScriptGeneric* gen) {
        Call<fp>(gen, std::index_sequence_for<A...>{});
    }
};

// --- free function, object pointer/ref as FIRST C++ param -----------------
template <typename Sig> struct ObjFirstFn;

template <typename R, typename T, typename... A>
struct ObjFirstFn<R (*)(T, A...)> {
    template <R (*fp)(T, A...), size_t... Is>
    static void Call(asIScriptGeneric* gen, std::index_sequence<Is...>) {
        if constexpr (std::is_void_v<R>) {
            fp(Proxy<T>::cast(gen->GetObject()),
               ArgValue<A>(gen, static_cast<asUINT>(Is))...);
        } else {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>{ fp(Proxy<T>::cast(gen->GetObject()),
                             ArgValue<A>(gen, static_cast<asUINT>(Is))...) };
        }
    }
    template <R (*fp)(T, A...)>
    static void Thunk(asIScriptGeneric* gen) {
        Call<fp>(gen, std::index_sequence_for<A...>{});
    }
};

// --- free function, object pointer/ref as LAST C++ param ------------------
template <typename Sig> struct ObjLastFn;

template <typename R, typename... P>
struct ObjLastFn<R (*)(P...)> {
    static_assert(sizeof...(P) >= 1, "OBJ_LAST needs at least the object param");
    using Tup  = std::tuple<P...>;
    using ObjT = std::tuple_element_t<sizeof...(P) - 1, Tup>;

    template <R (*fp)(P...), size_t... Is>
    static void Call(asIScriptGeneric* gen, std::index_sequence<Is...>) {
        if constexpr (std::is_void_v<R>) {
            fp(ArgValue<std::tuple_element_t<Is, Tup>>(gen, static_cast<asUINT>(Is))...,
               Proxy<ObjT>::cast(gen->GetObject()));
        } else {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>{ fp(ArgValue<std::tuple_element_t<Is, Tup>>(gen, static_cast<asUINT>(Is))...,
                             Proxy<ObjT>::cast(gen->GetObject())) };
        }
    }
    template <R (*fp)(P...)>
    static void Thunk(asIScriptGeneric* gen) {
        Call<fp>(gen, std::make_index_sequence<sizeof...(P) - 1>{});
    }
};

// --- member function: R (C::*)(A...) [const] ------------------------------
template <typename Sig> struct MemberFn;

template <typename C, typename R, typename... A>
struct MemberFn<R (C::*)(A...)> {
    template <R (C::*fp)(A...), size_t... Is>
    static void Call(asIScriptGeneric* gen, std::index_sequence<Is...>) {
        C* obj = static_cast<C*>(gen->GetObject());
        if constexpr (std::is_void_v<R>) {
            (obj->*fp)(ArgValue<A>(gen, static_cast<asUINT>(Is))...);
        } else {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>{ (obj->*fp)(ArgValue<A>(gen, static_cast<asUINT>(Is))...) };
        }
    }
    template <R (C::*fp)(A...)>
    static void Thunk(asIScriptGeneric* gen) {
        Call<fp>(gen, std::index_sequence_for<A...>{});
    }
};

template <typename C, typename R, typename... A>
struct MemberFn<R (C::*)(A...) const> {
    template <R (C::*fp)(A...) const, size_t... Is>
    static void Call(asIScriptGeneric* gen, std::index_sequence<Is...>) {
        const C* obj = static_cast<const C*>(gen->GetObject());
        if constexpr (std::is_void_v<R>) {
            (obj->*fp)(ArgValue<A>(gen, static_cast<asUINT>(Is))...);
        } else {
            new (gen->GetAddressOfReturnLocation())
                Proxy<R>{ (obj->*fp)(ArgValue<A>(gen, static_cast<asUINT>(Is))...) };
        }
    }
    template <R (C::*fp)(A...) const>
    static void Thunk(asIScriptGeneric* gen) {
        Call<fp>(gen, std::index_sequence_for<A...>{});
    }
};

// Deduction helpers: MakeId(f) captures the pointer TYPE so the macros can
// name the exact specialization without spelling the signature out.
template <typename T>
struct Id {
    template <T fp> static asSFuncPtr Fn()       { return asFunctionPtr(&FreeFn<T>::template Thunk<fp>); }
    template <T fp> static asSFuncPtr ObjFirst() { return asFunctionPtr(&ObjFirstFn<T>::template Thunk<fp>); }
    template <T fp> static asSFuncPtr ObjLast()  { return asFunctionPtr(&ObjLastFn<T>::template Thunk<fp>); }
    template <T fp> static asSFuncPtr Mfn()      { return asFunctionPtr(&MemberFn<T>::template Thunk<fp>); }
};

template <typename T> Id<T> MakeId(T) { return {}; }
// noexcept function pointers decay to their plain type for wrapping
template <typename R, typename... A> Id<R (*)(A...)> MakeId(R (*)(A...) noexcept) { return {}; }
template <typename C, typename R, typename... A> Id<R (C::*)(A...)> MakeId(R (C::*)(A...) noexcept) { return {}; }
template <typename C, typename R, typename... A> Id<R (C::*)(A...) const> MakeId(R (C::*)(A...) const noexcept) { return {}; }

} // namespace Enjin::Scripting::GenWrap

#define ENJIN_AS_FN(f)                 (::Enjin::Scripting::GenWrap::MakeId(f).template Fn<f>())
#define ENJIN_AS_OBJ_FIRST(f)          (::Enjin::Scripting::GenWrap::MakeId(f).template ObjFirst<f>())
#define ENJIN_AS_OBJ_LAST(f)           (::Enjin::Scripting::GenWrap::MakeId(f).template ObjLast<f>())
#define ENJIN_AS_MFN(c, m)             (::Enjin::Scripting::GenWrap::MakeId(&c::m).template Mfn<&c::m>())
#define ENJIN_AS_CALL_CDECL            asCALL_GENERIC
#define ENJIN_AS_CALL_CDECL_OBJFIRST   asCALL_GENERIC
#define ENJIN_AS_CALL_CDECL_OBJLAST    asCALL_GENERIC
#define ENJIN_AS_CALL_THISCALL         asCALL_GENERIC

#endif // AS_MAX_PORTABILITY
