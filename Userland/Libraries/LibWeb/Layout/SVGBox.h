/*
 * Copyright (c) 2020, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/BlockBox.h>
#include <LibWeb/SVG/SVGElement.h>
#include <LibWeb/SVG/SVGGraphicsElement.h>

namespace Web::Layout {

class SVGBox : public BlockBox {
public:
    SVGBox(DOM::Document&, SVG::SVGElement&, NonnullRefPtr<CSS::StyleProperties>);
    virtual ~SVGBox() override = default;

    SVG::SVGElement& dom_node() { return verify_cast<SVG::SVGElement>(*Box::dom_node()); }

    virtual void before_children_paint(PaintContext& context, PaintPhase phase) override;
    virtual void after_children_paint(PaintContext& context, PaintPhase phase) override;
};

}
