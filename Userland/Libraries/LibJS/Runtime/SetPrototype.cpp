/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashTable.h>
#include <AK/TypeCasts.h>
#include <LibJS/Runtime/SetIterator.h>
#include <LibJS/Runtime/SetPrototype.h>

namespace JS {

SetPrototype::SetPrototype(GlobalObject& global_object)
    : PrototypeObject(*global_object.object_prototype())
{
}

void SetPrototype::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    Object::initialize(global_object);
    u8 attr = Attribute::Writable | Attribute::Configurable;

    define_native_function(vm.names.add, add, 1, attr);
    define_native_function(vm.names.clear, clear, 0, attr);
    define_native_function(vm.names.delete_, delete_, 1, attr);
    define_native_function(vm.names.entries, entries, 0, attr);
    define_native_function(vm.names.forEach, for_each, 1, attr);
    define_native_function(vm.names.has, has, 1, attr);
    define_native_function(vm.names.values, values, 0, attr);
    define_native_accessor(vm.names.size, size_getter, {}, Attribute::Configurable);

    define_direct_property(vm.names.keys, get(vm.names.values), attr);

    // 24.2.3.11 Set.prototype [ @@iterator ] ( ), https://tc39.es/ecma262/#sec-set.prototype-@@iterator
    define_direct_property(*vm.well_known_symbol_iterator(), get(vm.names.values), attr);

    // 24.2.3.12 Set.prototype [ @@toStringTag ], https://tc39.es/ecma262/#sec-set.prototype-@@tostringtag
    define_direct_property(*vm.well_known_symbol_to_string_tag(), js_string(vm, vm.names.Set.as_string()), Attribute::Configurable);
}

SetPrototype::~SetPrototype()
{
}

// 24.2.3.1 Set.prototype.add ( value ), https://tc39.es/ecma262/#sec-set.prototype.add
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::add)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};
    auto value = vm.argument(0);
    if (value.is_negative_zero())
        value = Value(0);
    set->values().set(value, AK::HashSetExistingEntryBehavior::Keep);
    return set;
}

// 24.2.3.2 Set.prototype.clear ( ), https://tc39.es/ecma262/#sec-set.prototype.clear
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::clear)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};
    set->values().clear();
    return js_undefined();
}

// 24.2.3.4 Set.prototype.delete ( value ), https://tc39.es/ecma262/#sec-set.prototype.delete
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::delete_)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};
    return Value(set->values().remove(vm.argument(0)));
}

// 24.2.3.5 Set.prototype.entries ( ), https://tc39.es/ecma262/#sec-set.prototype.entries
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::entries)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};

    return SetIterator::create(global_object, *set, Object::PropertyKind::KeyAndValue);
}

// 24.2.3.6 Set.prototype.forEach ( callbackfn [ , thisArg ] ), https://tc39.es/ecma262/#sec-set.prototype.foreach
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::for_each)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};
    if (!vm.argument(0).is_function()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::NotAFunction, vm.argument(0).to_string_without_side_effects());
        return {};
    }
    auto this_value = vm.this_value(global_object);
    for (auto& value : set->values()) {
        (void)vm.call(vm.argument(0).as_function(), vm.argument(1), value, value, this_value);
        if (vm.exception())
            return {};
    }
    return js_undefined();
}

// 24.2.3.7 Set.prototype.has ( value ), https://tc39.es/ecma262/#sec-set.prototype.has
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::has)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};
    auto& values = set->values();
    return Value(values.find(vm.argument(0)) != values.end());
}

// 24.2.3.10 Set.prototype.values ( ), https://tc39.es/ecma262/#sec-set.prototype.values
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::values)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};

    return SetIterator::create(global_object, *set, Object::PropertyKind::Value);
}

// 24.2.3.9 get Set.prototype.size, https://tc39.es/ecma262/#sec-get-set.prototype.size
JS_DEFINE_NATIVE_GETTER(SetPrototype::size_getter)
{
    auto* set = typed_this_object(global_object);
    if (!set)
        return {};
    return Value(set->values().size());
}

}
