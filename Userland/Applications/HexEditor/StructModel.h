/*
 * Copyright (c) 2021, Arne Elster <arne@elster.li>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibCpp/AST.h>
#include <LibGUI/Model.h>

namespace HexEditor {

class StructModel final : public GUI::Model {
public:
    static NonnullRefPtr<StructModel> create(Cpp::Type& struct_type)
    {
        return adopt_ref(*new StructModel(struct_type));
    }

    enum Column {
        Member,
        Value,
        __Count
    };

    virtual ~StructModel() override;

    virtual int row_count(const GUI::ModelIndex& = GUI::ModelIndex()) const override;
    virtual int column_count(const GUI::ModelIndex& = GUI::ModelIndex()) const override;
    virtual String column_name(int) const override;
    virtual GUI::Variant data(const GUI::ModelIndex&, GUI::ModelRole) const override;
    virtual GUI::ModelIndex index(int row, int column, const GUI::ModelIndex& parent = GUI::ModelIndex()) const override;
    virtual GUI::ModelIndex parent_index(const GUI::ModelIndex&) const override;
    virtual int tree_column() const override { return Column::Member; }
    virtual bool is_column_sortable(int) const override { return false; }
    virtual bool is_searchable() const override { return true; }
    virtual Vector<GUI::ModelIndex> matches(StringView, unsigned flags, GUI::ModelIndex const&) override;

private:
    explicit StructModel(Cpp::Type&);
};

}
