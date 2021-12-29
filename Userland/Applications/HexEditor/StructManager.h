/*
 * Copyright (c) 2021, Arne Elster <arne@elster.li>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <AK/NonnullRefPtrVector.h>
#include <AK/String.h>
#include <AK/Variant.h>
#include <AK/Vector.h>

class Struct;

class Type : public RefCounted<Type> {
public:
    enum class Kind {
        Intrinsic,
        Array,
        Struct,
        UnknownIdentifier
    };

    Type(Kind kind)
        : m_kind(kind)
    {
    }

    virtual ~Type() { }

    Kind type_kind() const { return m_kind; }

    virtual StringView name() const = 0;
    virtual size_t size_in_bytes() const = 0;

private:
    Kind m_kind;
};

class IntrinsicType : public Type {
public:
    enum class Kind {
        Char,
        Short,
        Int,
        Long,
        Float,
        Double,
        LongLong,
        ShortInt,
        LongInt,
        LongDouble
    };

    // FIXME: Find pattern to allow protected constructor with make<>/make_ref_counted<>
    IntrinsicType(StringView name, Kind kind, bool is_unsigned)
        : Type(Type::Kind::Intrinsic)
        , m_name(name)
        , m_kind(kind)
        , m_unsigned(is_unsigned)
    {
    }

    static ErrorOr<NonnullRefPtr<IntrinsicType>> create(StringView type_name)
    {
        auto parts = type_name.split_view(' ');
        bool is_unsigned = parts.find("unsigned").is_end();

        parts.remove_all_matching([](auto& part) { return part == "unsigned" || part == "signed"; });
        auto name = String::join(' ', parts);

        Kind kind;
        if (name == "long double"sv || name == "double long"sv)
            kind = Kind::LongDouble;
        else if (name == "long int"sv || name == "int long"sv)
            kind = Kind::LongInt;
        else if (name == "short int"sv || name == "int short"sv)
            kind = Kind::ShortInt;
        else if (name == "long long"sv)
            kind = Kind::LongLong;
        else if (name == "double"sv)
            kind = Kind::Double;
        else if (name == "float"sv)
            kind = Kind::Float;
        else if (name == "long"sv)
            kind = Kind::Long;
        else if (name == "int"sv)
            kind = Kind::Int;
        else if (name == "short"sv)
            kind = Kind::Short;
        else if (name == "char"sv)
            kind = Kind::Char;
        else
            return Error::from_string_literal("Unknown type");

        return make_ref_counted<IntrinsicType>(type_name, kind, is_unsigned);
    }

    virtual StringView name() const override { return m_name; }

    virtual Kind intrinsic_type_kind() const { return m_kind; }

    bool is_unsigned() const { return m_unsigned; }

    virtual size_t size_in_bytes() const override
    {
        switch (m_kind) {
        case Kind::Char:
            return sizeof(char);
        case Kind::Double:
            return sizeof(double);
        case Kind::Float:
            return sizeof(float);
        case Kind::Int:
            return sizeof(int);
        case Kind::Long:
            return sizeof(long);
        case Kind::LongDouble:
            return sizeof(long double);
        case Kind::LongInt:
            return sizeof(long int);
        case Kind::LongLong:
            return sizeof(long long);
        case Kind::Short:
            return sizeof(short);
        case Kind::ShortInt:
            return sizeof(short int);
        default:
            VERIFY_NOT_REACHED();
        }
    }

private:
    String m_name;
    Kind m_kind;
    bool m_unsigned;
};

class ArrayType : public Type {
public:
    ArrayType(size_t size, NonnullRefPtr<Type> base_type)
        : Type(Type::Kind::Array)
        , m_size(size)
        , m_type(base_type)
    {
        m_name = String::formatted("{}[{}]", m_type->name(), m_size);
    }

    size_t size() const { return m_size; }
    NonnullRefPtr<Type const> type() const { return m_type; }

    virtual StringView name() const override { return m_name; }
    virtual size_t size_in_bytes() const override { return size() * type()->size_in_bytes(); }

private:
    size_t m_size;
    NonnullRefPtr<Type> m_type;
    String m_name;
};

class StructMember {
public:
    StructMember(StringView name, NonnullRefPtr<Type> type)
        : m_name(name)
        , m_type(type)
        , m_offset(0)
    {
    }

    String name() const { return m_name; }
    void set_name(StringView name) { m_name = name; }
    Type const& type() const { return *m_type; }
    void set_type(NonnullRefPtr<Type> type) { m_type = type; }
    size_t offset() const { return m_offset; }
    void set_offset(size_t offset) { m_offset = offset; }

private:
    String m_name;
    NonnullRefPtr<Type> m_type;
    size_t m_offset;
};

class Struct : public Type {
public:
    Struct(StringView name)
        : Type(Type::Kind::Struct)
        , m_name(name)
    {
    }

    virtual StringView name() const override { return m_name; }

    virtual size_t size_in_bytes() const override
    {
        size_t sum = 0;
        for (auto& member : m_members) {
            sum += member.type().size_in_bytes();
        }
        return sum;
    }

    Vector<StructMember>::ConstIterator begin() const { return m_members.begin(); }
    Vector<StructMember>::ConstIterator end() const { return m_members.end(); }
    size_t size() const { return m_members.size(); }
    StructMember const& at(size_t i) { return m_members.at(i); }
    void add_member(StructMember member)
    {
        size_t offset = size();
        member.set_offset(offset);
        m_members.append(member);
    }

private:
    String m_name;
    Vector<StructMember> m_members;
};

class StructManager {
public:
    void add(NonnullRefPtr<Struct> structure)
    {
        m_structs.append(structure);
    }

    NonnullRefPtrVector<Struct>::ConstIterator begin() const { return m_structs.begin(); }
    NonnullRefPtrVector<Struct>::ConstIterator end() const { return m_structs.end(); }
    size_t size() const { return m_structs.size(); }
    Struct const& at(size_t i) { return m_structs.at(i); }

    Optional<NonnullRefPtr<Struct const>> find(StringView struct_name)
    {
        auto result = m_structs.first_matching([struct_name](auto structure) { return structure->name() == struct_name; });
        if (result.has_value()) {
            // Hack to make the inner const cast
            return result.value();
        }
        return {};
    }

private:
    NonnullRefPtrVector<Struct> m_structs;
};
