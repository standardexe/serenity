/*
 * Copyright (c) 2020, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/ExtraMathConstants.h>
#include <AK/StringBuilder.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Path.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Event.h>
#include <LibWeb/Layout/SVGPathBox.h>
#include <LibWeb/SVG/SVGPathElement.h>
#include <ctype.h>

namespace Web::SVG {

[[maybe_unused]] static void print_instruction(const PathInstruction& instruction)
{
    VERIFY(PATH_DEBUG);

    auto& data = instruction.data;

    switch (instruction.type) {
    case PathInstructionType::Move:
        dbgln("Move (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 2)
            dbgln("    x={}, y={}", data[i], data[i + 1]);
        break;
    case PathInstructionType::ClosePath:
        dbgln("ClosePath (absolute={})", instruction.absolute);
        break;
    case PathInstructionType::Line:
        dbgln("Line (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 2)
            dbgln("    x={}, y={}", data[i], data[i + 1]);
        break;
    case PathInstructionType::HorizontalLine:
        dbgln("HorizontalLine (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); ++i)
            dbgln("    x={}", data[i]);
        break;
    case PathInstructionType::VerticalLine:
        dbgln("VerticalLine (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); ++i)
            dbgln("    y={}", data[i]);
        break;
    case PathInstructionType::Curve:
        dbgln("Curve (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 6)
            dbgln("    (x1={}, y1={}, x2={}, y2={}), (x={}, y={})", data[i], data[i + 1], data[i + 2], data[i + 3], data[i + 4], data[i + 5]);
        break;
    case PathInstructionType::SmoothCurve:
        dbgln("SmoothCurve (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 4)
            dbgln("    (x2={}, y2={}), (x={}, y={})", data[i], data[i + 1], data[i + 2], data[i + 3]);
        break;
    case PathInstructionType::QuadraticBezierCurve:
        dbgln("QuadraticBezierCurve (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 4)
            dbgln("    (x1={}, y1={}), (x={}, y={})", data[i], data[i + 1], data[i + 2], data[i + 3]);
        break;
    case PathInstructionType::SmoothQuadraticBezierCurve:
        dbgln("SmoothQuadraticBezierCurve (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 2)
            dbgln("    x={}, y={}", data[i], data[i + 1]);
        break;
    case PathInstructionType::EllipticalArc:
        dbgln("EllipticalArc (absolute={})", instruction.absolute);
        for (size_t i = 0; i < data.size(); i += 7)
            dbgln("    (rx={}, ry={}) x-axis-rotation={}, large-arc-flag={}, sweep-flag={}, (x={}, y={})",
                data[i],
                data[i + 1],
                data[i + 2],
                data[i + 3],
                data[i + 4],
                data[i + 5],
                data[i + 6]);
        break;
    case PathInstructionType::Invalid:
        dbgln("Invalid");
        break;
    }
}

PathDataParser::PathDataParser(const String& source)
    : m_source(source)
{
}

Vector<PathInstruction> PathDataParser::parse()
{
    parse_whitespace();
    while (!done())
        parse_drawto();
    if (!m_instructions.is_empty() && m_instructions[0].type != PathInstructionType::Move)
        VERIFY_NOT_REACHED();
    return m_instructions;
}

void PathDataParser::parse_drawto()
{
    if (match('M') || match('m')) {
        parse_moveto();
    } else if (match('Z') || match('z')) {
        parse_closepath();
    } else if (match('L') || match('l')) {
        parse_lineto();
    } else if (match('H') || match('h')) {
        parse_horizontal_lineto();
    } else if (match('V') || match('v')) {
        parse_vertical_lineto();
    } else if (match('C') || match('c')) {
        parse_curveto();
    } else if (match('S') || match('s')) {
        parse_smooth_curveto();
    } else if (match('Q') || match('q')) {
        parse_quadratic_bezier_curveto();
    } else if (match('T') || match('t')) {
        parse_smooth_quadratic_bezier_curveto();
    } else if (match('A') || match('a')) {
        parse_elliptical_arc();
    } else {
        dbgln("PathDataParser::parse_drawto failed to match: '{}'", ch());
        TODO();
    }
}

void PathDataParser::parse_moveto()
{
    bool absolute = consume() == 'M';
    parse_whitespace();
    for (auto& pair : parse_coordinate_pair_sequence())
        m_instructions.append({ PathInstructionType::Move, absolute, pair });
}

void PathDataParser::parse_closepath()
{
    bool absolute = consume() == 'Z';
    parse_whitespace();
    m_instructions.append({ PathInstructionType::ClosePath, absolute, {} });
}

void PathDataParser::parse_lineto()
{
    bool absolute = consume() == 'L';
    parse_whitespace();
    for (auto& pair : parse_coordinate_pair_sequence())
        m_instructions.append({ PathInstructionType::Line, absolute, pair });
}

void PathDataParser::parse_horizontal_lineto()
{
    bool absolute = consume() == 'H';
    parse_whitespace();
    m_instructions.append({ PathInstructionType::HorizontalLine, absolute, parse_coordinate_sequence() });
}

void PathDataParser::parse_vertical_lineto()
{
    bool absolute = consume() == 'V';
    parse_whitespace();
    m_instructions.append({ PathInstructionType::VerticalLine, absolute, parse_coordinate_sequence() });
}

void PathDataParser::parse_curveto()
{
    bool absolute = consume() == 'C';
    parse_whitespace();

    while (true) {
        m_instructions.append({ PathInstructionType::Curve, absolute, parse_coordinate_pair_triplet() });
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_coordinate())
            break;
    }
}

void PathDataParser::parse_smooth_curveto()
{
    bool absolute = consume() == 'S';
    parse_whitespace();

    while (true) {
        m_instructions.append({ PathInstructionType::SmoothCurve, absolute, parse_coordinate_pair_double() });
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_coordinate())
            break;
    }
}

void PathDataParser::parse_quadratic_bezier_curveto()
{
    bool absolute = consume() == 'Q';
    parse_whitespace();

    while (true) {
        m_instructions.append({ PathInstructionType::QuadraticBezierCurve, absolute, parse_coordinate_pair_double() });
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_coordinate())
            break;
    }
}

void PathDataParser::parse_smooth_quadratic_bezier_curveto()
{
    bool absolute = consume() == 'T';
    parse_whitespace();

    while (true) {
        m_instructions.append({ PathInstructionType::SmoothQuadraticBezierCurve, absolute, parse_coordinate_pair() });
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_coordinate())
            break;
    }
}

void PathDataParser::parse_elliptical_arc()
{
    bool absolute = consume() == 'A';
    parse_whitespace();

    while (true) {
        m_instructions.append({ PathInstructionType::EllipticalArc, absolute, parse_elliptical_arg_argument() });
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_coordinate())
            break;
    }
}

float PathDataParser::parse_coordinate()
{
    return parse_sign() * parse_number();
}

Vector<float> PathDataParser::parse_coordinate_pair()
{
    Vector<float> coordinates;
    coordinates.append(parse_coordinate());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    coordinates.append(parse_coordinate());
    return coordinates;
}

Vector<float> PathDataParser::parse_coordinate_sequence()
{
    Vector<float> sequence;
    while (true) {
        sequence.append(parse_coordinate());
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_comma_whitespace() && !match_coordinate())
            break;
    }
    return sequence;
}

Vector<Vector<float>> PathDataParser::parse_coordinate_pair_sequence()
{
    Vector<Vector<float>> sequence;
    while (true) {
        sequence.append(parse_coordinate_pair());
        if (match_comma_whitespace())
            parse_comma_whitespace();
        if (!match_comma_whitespace() && !match_coordinate())
            break;
    }
    return sequence;
}

Vector<float> PathDataParser::parse_coordinate_pair_double()
{
    Vector<float> coordinates;
    coordinates.extend(parse_coordinate_pair());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    coordinates.extend(parse_coordinate_pair());
    return coordinates;
}

Vector<float> PathDataParser::parse_coordinate_pair_triplet()
{
    Vector<float> coordinates;
    coordinates.extend(parse_coordinate_pair());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    coordinates.extend(parse_coordinate_pair());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    coordinates.extend(parse_coordinate_pair());
    return coordinates;
}

Vector<float> PathDataParser::parse_elliptical_arg_argument()
{
    Vector<float> numbers;
    numbers.append(parse_number());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    numbers.append(parse_number());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    numbers.append(parse_number());
    parse_comma_whitespace();
    numbers.append(parse_flag());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    numbers.append(parse_flag());
    if (match_comma_whitespace())
        parse_comma_whitespace();
    numbers.extend(parse_coordinate_pair());

    return numbers;
}

void PathDataParser::parse_whitespace(bool must_match_once)
{
    bool matched = false;
    while (!done() && match_whitespace()) {
        consume();
        matched = true;
    }

    VERIFY(!must_match_once || matched);
}

void PathDataParser::parse_comma_whitespace()
{
    if (match(',')) {
        consume();
        parse_whitespace();
    } else {
        parse_whitespace(1);
        if (match(','))
            consume();
        parse_whitespace();
    }
}

float PathDataParser::parse_fractional_constant()
{
    StringBuilder builder;
    bool floating_point = false;

    while (!done() && isdigit(ch()))
        builder.append(consume());

    if (match('.')) {
        floating_point = true;
        builder.append('.');
        consume();
        while (!done() && isdigit(ch()))
            builder.append(consume());
    } else {
        VERIFY(builder.length() > 0);
    }

    if (floating_point)
        return strtof(builder.to_string().characters(), nullptr);
    return builder.to_string().to_int().value();
}

float PathDataParser::parse_number()
{
    auto number = parse_fractional_constant();

    if (!match('e') && !match('E'))
        return number;
    consume();

    auto exponent_sign = parse_sign();

    StringBuilder exponent_builder;
    while (!done() && isdigit(ch()))
        exponent_builder.append(consume());
    VERIFY(exponent_builder.length() > 0);

    auto exponent = exponent_builder.to_string().to_int().value();

    // Fast path: If the number is 0, there's no point in computing the exponentiation.
    if (number == 0)
        return number;

    if (exponent_sign < 0) {
        for (int i = 0; i < exponent; ++i) {
            number /= 10;
        }
    } else if (exponent_sign > 0) {
        for (int i = 0; i < exponent; ++i) {
            number *= 10;
        }
    }

    return number;
}

float PathDataParser::parse_flag()
{
    if (!match('0') && !match('1'))
        VERIFY_NOT_REACHED();
    return consume() - '0';
}

int PathDataParser::parse_sign()
{
    if (match('-')) {
        consume();
        return -1;
    }
    if (match('+'))
        consume();
    return 1;
}

bool PathDataParser::match_whitespace() const
{
    if (done())
        return false;
    char c = ch();
    return c == 0x9 || c == 0x20 || c == 0xa || c == 0xc || c == 0xd;
}

bool PathDataParser::match_comma_whitespace() const
{
    return match_whitespace() || match(',');
}

bool PathDataParser::match_coordinate() const
{
    return !done() && (isdigit(ch()) || ch() == '-' || ch() == '+' || ch() == '.');
}

SVGPathElement::SVGPathElement(DOM::Document& document, QualifiedName qualified_name)
    : SVGGeometryElement(document, move(qualified_name))
{
}

RefPtr<Layout::Node> SVGPathElement::create_layout_node()
{
    auto style = document().style_resolver().resolve_style(*this);
    if (style->display() == CSS::Display::None)
        return nullptr;
    return adopt_ref(*new Layout::SVGPathBox(document(), *this, move(style)));
}

void SVGPathElement::parse_attribute(const FlyString& name, const String& value)
{
    SVGGeometryElement::parse_attribute(name, value);

    if (name == "d")
        m_instructions = PathDataParser(value).parse();
}

Gfx::Path& SVGPathElement::get_path()
{
    if (m_path.has_value())
        return m_path.value();

    Gfx::Path path;

    for (auto& instruction : m_instructions) {
        auto& absolute = instruction.absolute;
        auto& data = instruction.data;

        if constexpr (PATH_DEBUG) {
            print_instruction(instruction);
        }

        bool clear_last_control_point = true;

        switch (instruction.type) {
        case PathInstructionType::Move: {
            Gfx::FloatPoint point = { data[0], data[1] };
            if (absolute) {
                path.move_to(point);
            } else {
                VERIFY(!path.segments().is_empty());
                path.move_to(point + path.segments().last().point());
            }
            break;
        }
        case PathInstructionType::ClosePath:
            path.close();
            break;
        case PathInstructionType::Line: {
            Gfx::FloatPoint point = { data[0], data[1] };
            if (absolute) {
                path.line_to(point);
            } else {
                VERIFY(!path.segments().is_empty());
                path.line_to(point + path.segments().last().point());
            }
            break;
        }
        case PathInstructionType::HorizontalLine: {
            VERIFY(!path.segments().is_empty());
            auto last_point = path.segments().last().point();
            if (absolute) {
                path.line_to(Gfx::FloatPoint { data[0], last_point.y() });
            } else {
                path.line_to(Gfx::FloatPoint { data[0] + last_point.x(), last_point.y() });
            }
            break;
        }
        case PathInstructionType::VerticalLine: {
            VERIFY(!path.segments().is_empty());
            auto last_point = path.segments().last().point();
            if (absolute) {
                path.line_to(Gfx::FloatPoint { last_point.x(), data[0] });
            } else {
                path.line_to(Gfx::FloatPoint { last_point.x(), data[0] + last_point.y() });
            }
            break;
        }
        case PathInstructionType::EllipticalArc: {
            double rx = data[0];
            double ry = data[1];
            double x_axis_rotation = double { data[2] } * M_DEG2RAD;
            double large_arc_flag = data[3];
            double sweep_flag = data[4];
            auto& last_point = path.segments().last().point();

            Gfx::FloatPoint next_point;

            if (absolute) {
                next_point = { data[5], data[6] };
            } else {
                next_point = { data[5] + last_point.x(), data[6] + last_point.y() };
            }

            path.elliptical_arc_to(next_point, { rx, ry }, x_axis_rotation, large_arc_flag != 0, sweep_flag != 0);

            break;
        }
        case PathInstructionType::QuadraticBezierCurve: {
            clear_last_control_point = false;

            Gfx::FloatPoint through = { data[0], data[1] };
            Gfx::FloatPoint point = { data[2], data[3] };

            if (absolute) {
                path.quadratic_bezier_curve_to(through, point);
                m_previous_control_point = through;
            } else {
                VERIFY(!path.segments().is_empty());
                auto last_point = path.segments().last().point();
                auto control_point = through + last_point;
                path.quadratic_bezier_curve_to(control_point, point + last_point);
                m_previous_control_point = control_point;
            }
            break;
        }
        case PathInstructionType::SmoothQuadraticBezierCurve: {
            clear_last_control_point = false;

            VERIFY(!path.segments().is_empty());
            auto last_point = path.segments().last().point();

            if (m_previous_control_point.is_null()) {
                m_previous_control_point = last_point;
            }

            auto dx_end_control = last_point.dx_relative_to(m_previous_control_point);
            auto dy_end_control = last_point.dy_relative_to(m_previous_control_point);
            auto control_point = Gfx::FloatPoint { last_point.x() + dx_end_control, last_point.y() + dy_end_control };

            Gfx::FloatPoint end_point = { data[0], data[1] };

            if (absolute) {
                path.quadratic_bezier_curve_to(control_point, end_point);
            } else {
                path.quadratic_bezier_curve_to(control_point, end_point + last_point);
            }

            m_previous_control_point = control_point;
            break;
        }

        case PathInstructionType::Curve: {
            Gfx::FloatPoint c1 = { data[0], data[1] };
            Gfx::FloatPoint c2 = { data[2], data[3] };
            Gfx::FloatPoint p2 = { data[4], data[5] };
            if (!absolute) {
                p2 += path.segments().last().point();
                c1 += path.segments().last().point();
                c2 += path.segments().last().point();
            }
            path.cubic_bezier_curve_to(c1, c2, p2);
            break;
        }

        case PathInstructionType::SmoothCurve:
            // Instead of crashing the browser every time we come across an SVG
            // with these path instructions, let's just skip them
            continue;
        case PathInstructionType::Invalid:
            VERIFY_NOT_REACHED();
        }

        if (clear_last_control_point) {
            m_previous_control_point = Gfx::FloatPoint {};
        }
    }

    m_path = path;
    return m_path.value();
}

}
