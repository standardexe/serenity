/*
 * Copyright (c) 2021, Tim Flynn <trflynn89@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Intl/AbstractOperations.h>
#include <LibJS/Runtime/Intl/NumberFormat.h>
#include <LibUnicode/CurrencyCode.h>
#include <LibUnicode/Locale.h>

namespace JS::Intl {

Vector<StringView> const& NumberFormat::relevant_extension_keys()
{
    // 15.3.3 Internal slots, https://tc39.es/ecma402/#sec-intl.numberformat-internal-slots
    // The value of the [[RelevantExtensionKeys]] internal slot is « "nu" ».
    static Vector<StringView> relevant_extension_keys { "nu"sv };
    return relevant_extension_keys;
}

// 15 NumberFormat Objects, https://tc39.es/ecma402/#numberformat-objects
NumberFormat::NumberFormat(Object& prototype)
    : Object(prototype)
{
}

void NumberFormat::set_style(StringView style)
{
    if (style == "decimal"sv)
        m_style = Style::Decimal;
    else if (style == "percent"sv)
        m_style = Style::Percent;
    else if (style == "currency"sv)
        m_style = Style::Currency;
    else if (style == "unit"sv)
        m_style = Style::Unit;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::style_string() const
{
    switch (m_style) {
    case Style::Decimal:
        return "decimal"sv;
    case Style::Percent:
        return "percent"sv;
    case Style::Currency:
        return "currency"sv;
    case Style::Unit:
        return "unit"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

void NumberFormat::set_currency_display(StringView currency_display)
{
    if (currency_display == "code"sv)
        m_currency_display = CurrencyDisplay::Code;
    else if (currency_display == "symbol"sv)
        m_currency_display = CurrencyDisplay::Symbol;
    else if (currency_display == "narrowSymbol"sv)
        m_currency_display = CurrencyDisplay::NarrowSymbol;
    else if (currency_display == "name"sv)
        m_currency_display = CurrencyDisplay::Name;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::currency_display_string() const
{
    VERIFY(m_currency_display.has_value());

    switch (*m_currency_display) {
    case CurrencyDisplay::Code:
        return "code"sv;
    case CurrencyDisplay::Symbol:
        return "symbol"sv;
    case CurrencyDisplay::NarrowSymbol:
        return "narrowSymbol"sv;
    case CurrencyDisplay::Name:
        return "name"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

void NumberFormat::set_currency_sign(StringView currency_sign)
{
    if (currency_sign == "standard"sv)
        m_currency_sign = CurrencySign::Standard;
    else if (currency_sign == "accounting"sv)
        m_currency_sign = CurrencySign::Accounting;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::currency_sign_string() const
{
    VERIFY(m_currency_sign.has_value());

    switch (*m_currency_sign) {
    case CurrencySign::Standard:
        return "standard"sv;
    case CurrencySign::Accounting:
        return "accounting"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

void NumberFormat::set_unit_display(StringView unit_display)
{
    if (unit_display == "short"sv)
        m_unit_display = UnitDisplay::Short;
    else if (unit_display == "narrow"sv)
        m_unit_display = UnitDisplay::Narrow;
    else if (unit_display == "long"sv)
        m_unit_display = UnitDisplay::Long;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::unit_display_string() const
{
    VERIFY(m_unit_display.has_value());

    switch (*m_unit_display) {
    case UnitDisplay::Short:
        return "short"sv;
    case UnitDisplay::Narrow:
        return "narrow"sv;
    case UnitDisplay::Long:
        return "long"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

StringView NumberFormat::rounding_type_string() const
{
    switch (m_rounding_type) {
    case RoundingType::SignificantDigits:
        return "significantDigits"sv;
    case RoundingType::FractionDigits:
        return "fractionDigits"sv;
    case RoundingType::CompactRounding:
        return "compactRounding"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

void NumberFormat::set_notation(StringView notation)
{
    if (notation == "standard"sv)
        m_notation = Notation::Standard;
    else if (notation == "scientific"sv)
        m_notation = Notation::Scientific;
    else if (notation == "engineering"sv)
        m_notation = Notation::Engineering;
    else if (notation == "compact"sv)
        m_notation = Notation::Compact;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::notation_string() const
{
    switch (m_notation) {
    case Notation::Standard:
        return "standard"sv;
    case Notation::Scientific:
        return "scientific"sv;
    case Notation::Engineering:
        return "engineering"sv;
    case Notation::Compact:
        return "compact"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

void NumberFormat::set_compact_display(StringView compact_display)
{
    if (compact_display == "short"sv)
        m_compact_display = CompactDisplay::Short;
    else if (compact_display == "long"sv)
        m_compact_display = CompactDisplay::Long;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::compact_display_string() const
{
    VERIFY(m_compact_display.has_value());

    switch (*m_compact_display) {
    case CompactDisplay::Short:
        return "short"sv;
    case CompactDisplay::Long:
        return "long"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

void NumberFormat::set_sign_display(StringView sign_display)
{
    if (sign_display == "auto"sv)
        m_sign_display = SignDisplay::Auto;
    else if (sign_display == "never"sv)
        m_sign_display = SignDisplay::Never;
    else if (sign_display == "always"sv)
        m_sign_display = SignDisplay::Always;
    else if (sign_display == "exceptZero"sv)
        m_sign_display = SignDisplay::ExceptZero;
    else
        VERIFY_NOT_REACHED();
}

StringView NumberFormat::sign_display_string() const
{
    switch (m_sign_display) {
    case SignDisplay::Auto:
        return "auto"sv;
    case SignDisplay::Never:
        return "never"sv;
    case SignDisplay::Always:
        return "always"sv;
    case SignDisplay::ExceptZero:
        return "exceptZero"sv;
    default:
        VERIFY_NOT_REACHED();
    }
}

// 15.1.1 SetNumberFormatDigitOptions ( intlObj, options, mnfdDefault, mxfdDefault, notation ), https://tc39.es/ecma402/#sec-setnfdigitoptions
ThrowCompletionOr<void> set_number_format_digit_options(GlobalObject& global_object, NumberFormat& intl_object, Object const& options, int default_min_fraction_digits, int default_max_fraction_digits, NumberFormat::Notation notation)
{
    auto& vm = global_object.vm();

    // 1. Assert: Type(intlObj) is Object.
    // 2. Assert: Type(options) is Object.
    // 3. Assert: Type(mnfdDefault) is Number.
    // 4. Assert: Type(mxfdDefault) is Number.

    // 5. Let mnid be ? GetNumberOption(options, "minimumIntegerDigits,", 1, 21, 1).
    auto min_integer_digits = TRY(get_number_option(global_object, options, vm.names.minimumIntegerDigits, 1, 21, 1));

    // 6. Let mnfd be ? Get(options, "minimumFractionDigits").
    auto min_fraction_digits = options.get(vm.names.minimumFractionDigits);
    if (auto* exception = vm.exception())
        return throw_completion(exception->value());

    // 7. Let mxfd be ? Get(options, "maximumFractionDigits").
    auto max_fraction_digits = options.get(vm.names.maximumFractionDigits);
    if (auto* exception = vm.exception())
        return throw_completion(exception->value());

    // 8. Let mnsd be ? Get(options, "minimumSignificantDigits").
    auto min_significant_digits = options.get(vm.names.minimumSignificantDigits);
    if (auto* exception = vm.exception())
        return throw_completion(exception->value());

    // 9. Let mxsd be ? Get(options, "maximumSignificantDigits").
    auto max_significant_digits = options.get(vm.names.maximumSignificantDigits);
    if (auto* exception = vm.exception())
        return throw_completion(exception->value());

    // 10. Set intlObj.[[MinimumIntegerDigits]] to mnid.
    intl_object.set_min_integer_digits(*min_integer_digits);

    // 11. If mnsd is not undefined or mxsd is not undefined, then
    if (!min_significant_digits.is_undefined() || !max_significant_digits.is_undefined()) {
        // a. Set intlObj.[[RoundingType]] to significantDigits.
        intl_object.set_rounding_type(NumberFormat::RoundingType::SignificantDigits);

        // b. Let mnsd be ? DefaultNumberOption(mnsd, 1, 21, 1).
        auto min_digits = TRY(default_number_option(global_object, min_significant_digits, 1, 21, 1));

        // c. Let mxsd be ? DefaultNumberOption(mxsd, mnsd, 21, 21).
        auto max_digits = TRY(default_number_option(global_object, max_significant_digits, *min_digits, 21, 21));

        // d. Set intlObj.[[MinimumSignificantDigits]] to mnsd.
        intl_object.set_min_significant_digits(*min_digits);

        // e. Set intlObj.[[MaximumSignificantDigits]] to mxsd.
        intl_object.set_max_significant_digits(*max_digits);
    }
    // 12. Else if mnfd is not undefined or mxfd is not undefined, then
    else if (!min_fraction_digits.is_undefined() || !max_fraction_digits.is_undefined()) {
        // a. Set intlObj.[[RoundingType]] to fractionDigits.
        intl_object.set_rounding_type(NumberFormat::RoundingType::FractionDigits);

        // b. Let mnfd be ? DefaultNumberOption(mnfd, 0, 20, undefined).
        auto min_digits = TRY(default_number_option(global_object, min_fraction_digits, 0, 20, {}));

        // c. Let mxfd be ? DefaultNumberOption(mxfd, 0, 20, undefined).
        auto max_digits = TRY(default_number_option(global_object, max_fraction_digits, 0, 20, {}));

        // d. If mnfd is undefined, set mnfd to min(mnfdDefault, mxfd).
        if (!min_digits.has_value())
            min_digits = min(default_min_fraction_digits, *max_digits);
        // e. Else if mxfd is undefined, set mxfd to max(mxfdDefault, mnfd).
        else if (!max_digits.has_value())
            max_digits = max(default_max_fraction_digits, *min_digits);
        // f. Else if mnfd is greater than mxfd, throw a RangeError exception.
        else if (*min_digits > *max_digits)
            return vm.throw_completion<RangeError>(global_object, ErrorType::IntlMinimumExceedsMaximum, *min_digits, *max_digits);

        // g. Set intlObj.[[MinimumFractionDigits]] to mnfd.
        intl_object.set_min_fraction_digits(*min_digits);

        // h. Set intlObj.[[MaximumFractionDigits]] to mxfd.
        intl_object.set_max_fraction_digits(*max_digits);
    }
    // 13. Else if notation is "compact", then
    else if (notation == NumberFormat::Notation::Compact) {
        // a. Set intlObj.[[RoundingType]] to compactRounding.
        intl_object.set_rounding_type(NumberFormat::RoundingType::CompactRounding);

    }
    // 14. Else,
    else {
        // a. Set intlObj.[[RoundingType]] to fractionDigits.
        intl_object.set_rounding_type(NumberFormat::RoundingType::FractionDigits);

        // b. Set intlObj.[[MinimumFractionDigits]] to mnfdDefault.
        intl_object.set_min_fraction_digits(default_min_fraction_digits);

        // c. Set intlObj.[[MaximumFractionDigits]] to mxfdDefault.
        intl_object.set_max_fraction_digits(default_max_fraction_digits);
    }
    return {};
}

// 15.1.2 InitializeNumberFormat ( numberFormat, locales, options ), https://tc39.es/ecma402/#sec-initializenumberformat
ThrowCompletionOr<NumberFormat*> initialize_number_format(GlobalObject& global_object, NumberFormat& number_format, Value locales_value, Value options_value)
{
    auto& vm = global_object.vm();

    // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
    auto requested_locales = TRY(canonicalize_locale_list(global_object, locales_value));

    // 2. Set options to ? CoerceOptionsToObject(options).
    auto* options = TRY(coerce_options_to_object(global_object, options_value));

    // 3. Let opt be a new Record.
    LocaleOptions opt {};

    // 4. Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    auto matcher = TRY(get_option(global_object, *options, vm.names.localeMatcher, Value::Type::String, { "lookup"sv, "best fit"sv }, "best fit"sv));

    // 5. Set opt.[[localeMatcher]] to matcher.
    opt.locale_matcher = matcher;

    // 6. Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    auto numbering_system = TRY(get_option(global_object, *options, vm.names.numberingSystem, Value::Type::String, {}, Empty {}));

    // 7. If numberingSystem is not undefined, then
    if (!numbering_system.is_undefined()) {
        // a. If numberingSystem does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        if (!Unicode::is_type_identifier(numbering_system.as_string().string()))
            return vm.throw_completion<RangeError>(global_object, ErrorType::OptionIsNotValidValue, numbering_system, "numberingSystem"sv);

        // 8. Set opt.[[nu]] to numberingSystem.
        opt.nu = numbering_system.as_string().string();
    }

    // 9. Let localeData be %NumberFormat%.[[LocaleData]].
    // 10. Let r be ResolveLocale(%NumberFormat%.[[AvailableLocales]], requestedLocales, opt, %NumberFormat%.[[RelevantExtensionKeys]], localeData).
    auto result = resolve_locale(requested_locales, opt, NumberFormat::relevant_extension_keys());

    // 11. Set numberFormat.[[Locale]] to r.[[locale]].
    number_format.set_locale(move(result.locale));

    // 12. Set numberFormat.[[DataLocale]] to r.[[dataLocale]].
    number_format.set_data_locale(move(result.data_locale));

    // 13. Set numberFormat.[[NumberingSystem]] to r.[[nu]].
    number_format.set_numbering_system(result.nu.release_value());

    // 14. Perform ? SetNumberFormatUnitOptions(numberFormat, options).
    TRY(set_number_format_unit_options(global_object, number_format, *options));

    // 15. Let style be numberFormat.[[Style]].
    auto style = number_format.style();

    int default_min_fraction_digits = 0;
    int default_max_fraction_digits = 0;

    // 16. If style is "currency", then
    if (style == NumberFormat::Style::Currency) {
        // a. Let currency be numberFormat.[[Currency]].
        auto const& currency = number_format.currency();

        // b. Let cDigits be CurrencyDigits(currency).
        int digits = currency_digits(currency);

        // c. Let mnfdDefault be cDigits.
        default_min_fraction_digits = digits;

        // d. Let mxfdDefault be cDigits.
        default_max_fraction_digits = digits;
    }
    // 17. Else,
    else {
        // a. Let mnfdDefault be 0.
        default_min_fraction_digits = 0;

        // b. If style is "percent", then
        //     i. Let mxfdDefault be 0.
        // c. Else,
        //     i. Let mxfdDefault be 3.
        default_max_fraction_digits = style == NumberFormat::Style::Percent ? 0 : 3;
    }

    // 18. Let notation be ? GetOption(options, "notation", "string", « "standard", "scientific", "engineering", "compact" », "standard").
    auto notation = TRY(get_option(global_object, *options, vm.names.notation, Value::Type::String, { "standard"sv, "scientific"sv, "engineering"sv, "compact"sv }, "standard"sv));

    // 19. Set numberFormat.[[Notation]] to notation.
    number_format.set_notation(notation.as_string().string());

    // 20. Perform ? SetNumberFormatDigitOptions(numberFormat, options, mnfdDefault, mxfdDefault, notation).
    TRY(set_number_format_digit_options(global_object, number_format, *options, default_min_fraction_digits, default_max_fraction_digits, number_format.notation()));

    // 21. Let compactDisplay be ? GetOption(options, "compactDisplay", "string", « "short", "long" », "short").
    auto compact_display = TRY(get_option(global_object, *options, vm.names.compactDisplay, Value::Type::String, { "short"sv, "long"sv }, "short"sv));

    // 22. If notation is "compact", then
    if (number_format.notation() == NumberFormat::Notation::Compact) {
        // a. Set numberFormat.[[CompactDisplay]] to compactDisplay.
        number_format.set_compact_display(compact_display.as_string().string());
    }

    // 23. Let useGrouping be ? GetOption(options, "useGrouping", "boolean", undefined, true).
    auto use_grouping = TRY(get_option(global_object, *options, vm.names.useGrouping, Value::Type::Boolean, {}, true));

    // 24. Set numberFormat.[[UseGrouping]] to useGrouping.
    number_format.set_use_grouping(use_grouping.as_bool());

    // 25. Let signDisplay be ? GetOption(options, "signDisplay", "string", « "auto", "never", "always", "exceptZero" », "auto").
    auto sign_display = TRY(get_option(global_object, *options, vm.names.signDisplay, Value::Type::String, { "auto"sv, "never"sv, "always"sv, "exceptZero"sv }, "auto"sv));

    // 26. Set numberFormat.[[SignDisplay]] to signDisplay.
    number_format.set_sign_display(sign_display.as_string().string());

    // 27. Return numberFormat.
    return &number_format;
}

// 15.1.3 CurrencyDigits ( currency ), https://tc39.es/ecma402/#sec-currencydigits
int currency_digits(StringView currency)
{
    // 1. If the ISO 4217 currency and funds code list contains currency as an alphabetic code, return the minor
    //    unit value corresponding to the currency from the list; otherwise, return 2.
    if (auto currency_code = Unicode::get_currency_code(currency); currency_code.has_value())
        return currency_code->minor_unit.value_or(2);
    return 2;
}

// 15.1.13 SetNumberFormatUnitOptions ( intlObj, options ), https://tc39.es/ecma402/#sec-setnumberformatunitoptions
ThrowCompletionOr<void> set_number_format_unit_options(GlobalObject& global_object, NumberFormat& intl_object, Object const& options)
{
    auto& vm = global_object.vm();

    // 1. Assert: Type(intlObj) is Object.
    // 2. Assert: Type(options) is Object.

    // 3. Let style be ? GetOption(options, "style", "string", « "decimal", "percent", "currency", "unit" », "decimal").
    auto style = TRY(get_option(global_object, options, vm.names.style, Value::Type::String, { "decimal"sv, "percent"sv, "currency"sv, "unit"sv }, "decimal"sv));

    // 4. Set intlObj.[[Style]] to style.
    intl_object.set_style(style.as_string().string());

    // 5. Let currency be ? GetOption(options, "currency", "string", undefined, undefined).
    auto currency = TRY(get_option(global_object, options, vm.names.currency, Value::Type::String, {}, Empty {}));

    // 6. If currency is undefined, then
    if (currency.is_undefined()) {
        // a. If style is "currency", throw a TypeError exception.
        if (intl_object.style() == NumberFormat::Style::Currency)
            return vm.throw_completion<TypeError>(global_object, ErrorType::IntlOptionUndefined, "currency"sv, "style"sv, style);
    }
    // 7. Else,
    //     a. If the result of IsWellFormedCurrencyCode(currency) is false, throw a RangeError exception.
    else if (!is_well_formed_currency_code(currency.as_string().string()))
        return vm.throw_completion<RangeError>(global_object, ErrorType::OptionIsNotValidValue, currency, "currency"sv);

    // 8. Let currencyDisplay be ? GetOption(options, "currencyDisplay", "string", « "code", "symbol", "narrowSymbol", "name" », "symbol").
    auto currency_display = TRY(get_option(global_object, options, vm.names.currencyDisplay, Value::Type::String, { "code"sv, "symbol"sv, "narrowSymbol"sv, "name"sv }, "symbol"sv));

    // 9. Let currencySign be ? GetOption(options, "currencySign", "string", « "standard", "accounting" », "standard").
    auto currency_sign = TRY(get_option(global_object, options, vm.names.currencySign, Value::Type::String, { "standard"sv, "accounting"sv }, "standard"sv));

    // 10. Let unit be ? GetOption(options, "unit", "string", undefined, undefined).
    auto unit = TRY(get_option(global_object, options, vm.names.unit, Value::Type::String, {}, Empty {}));

    // 11. If unit is undefined, then
    if (unit.is_undefined()) {
        // a. If style is "unit", throw a TypeError exception.
        if (intl_object.style() == NumberFormat::Style::Unit)
            return vm.throw_completion<TypeError>(global_object, ErrorType::IntlOptionUndefined, "unit"sv, "style"sv, style);
    }
    // 12. Else,
    //     a. If the result of IsWellFormedUnitIdentifier(unit) is false, throw a RangeError exception.
    else if (!is_well_formed_unit_identifier(unit.as_string().string()))
        return vm.throw_completion<RangeError>(global_object, ErrorType::OptionIsNotValidValue, unit, "unit"sv);

    // 13. Let unitDisplay be ? GetOption(options, "unitDisplay", "string", « "short", "narrow", "long" », "short").
    auto unit_display = TRY(get_option(global_object, options, vm.names.unitDisplay, Value::Type::String, { "short"sv, "narrow"sv, "long"sv }, "short"sv));

    // 14. If style is "currency", then
    if (intl_object.style() == NumberFormat::Style::Currency) {
        // a. Let currency be the result of converting currency to upper case as specified in 6.1.
        // b. Set intlObj.[[Currency]] to currency.
        intl_object.set_currency(currency.as_string().string().to_uppercase());

        // c. Set intlObj.[[CurrencyDisplay]] to currencyDisplay.
        intl_object.set_currency_display(currency_display.as_string().string());

        // d. Set intlObj.[[CurrencySign]] to currencySign.
        intl_object.set_currency_sign(currency_sign.as_string().string());
    }

    // 15. If style is "unit", then
    if (intl_object.style() == NumberFormat::Style::Unit) {
        // a. Set intlObj.[[Unit]] to unit.
        intl_object.set_unit(unit.as_string().string());

        // b. Set intlObj.[[UnitDisplay]] to unitDisplay.
        intl_object.set_unit_display(unit_display.as_string().string());
    }
    return {};
}

}
