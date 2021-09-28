/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Arne Elster <arne@elster.li>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullOwnPtr.h>
#include <AK/StdLibExtras.h>
#include <AK/Tuple.h>
#include <AK/Types.h>
#include <AK/Vector.h>

#include "DataProvider.h"

struct HexByte {
    u8 value;
    bool changed;
};

class Interval {
public:
    Interval(size_t size, size_t provider_offset, NonnullRefPtr<IDataProvider> provider, bool changed = true)
        : m_size(size)
        , m_data_provider(provider)
        , m_data_provider_offset(provider_offset)
        , m_changed(changed)
    {
    }

    size_t size() const { return m_size; }
    NonnullRefPtr<IDataProvider> provider() const { return m_data_provider; }
    size_t provider_offset() const { return m_data_provider_offset; }
    bool changed() const { return m_changed; }

private:
    size_t m_size;
    NonnullRefPtr<IDataProvider> m_data_provider;
    size_t m_data_provider_offset;
    bool m_changed;
};

using IntervalList = Vector<Interval>;

class IntervalChange {
public:
    enum class ChangeType {
        Insert,
        Delete,
        Modify
    };

    IntervalChange(ChangeType type, size_t offset, size_t size)
        : m_type(type)
        , m_offset(offset)
        , m_size(size)
    {
    }

    virtual ~IntervalChange() = default;

    ChangeType type() const { return m_type; }
    size_t offset() const { return m_offset; }
    size_t size() const { return m_size; }

    virtual IntervalList apply(const IntervalList& intervals) = 0;

private:
    ChangeType m_type;
    size_t m_offset;
    size_t m_size;
};

class IntervalChangeModify;
class IntervalChangeInsert;
class IntervalChangeRemove;

class IntervalChangeModify : public IntervalChange {
public:
    IntervalChangeModify(NonnullRefPtr<IDataProvider> provider, size_t provider_offset, size_t offset, size_t size);
    virtual ~IntervalChangeModify() = default;

    NonnullRefPtr<IDataProvider> provider() const { return m_provider; }
    size_t provider_offset() const { return m_provider_offset; }

    IntervalList apply(const IntervalList& intervals) override;

private:
    NonnullRefPtr<IDataProvider> m_provider;
    size_t m_provider_offset;

    OwnPtr<IntervalChangeInsert> m_change_insert;
    OwnPtr<IntervalChangeRemove> m_change_remove;
};

class IntervalChangeInsert : public IntervalChange {
public:
    IntervalChangeInsert(NonnullRefPtr<IDataProvider> provider, size_t provider_offset, size_t offset, size_t size)
        : IntervalChange(ChangeType::Insert, offset, size)
        , m_provider(provider)
        , m_provider_offset(provider_offset)
    {
    }

    virtual ~IntervalChangeInsert() = default;

    NonnullRefPtr<IDataProvider> provider() const { return m_provider; }
    size_t provider_offset() const { return m_provider_offset; }

    IntervalList apply(const IntervalList& intervals) override;

private:
    NonnullRefPtr<IDataProvider> m_provider;
    size_t m_provider_offset;
};

class IntervalChangeRemove : public IntervalChange {
public:
    IntervalChangeRemove(size_t offset, size_t size)
        : IntervalChange(ChangeType::Delete, offset, size)
    {
    }

    virtual ~IntervalChangeRemove() = default;

    IntervalList apply(const IntervalList& intervals) override;
};

class IntervalTools {
public:
    static IntervalList from_array(ByteBuffer&& buffer);
    static IntervalList remove(const IntervalList& list, size_t offset, size_t length);
    static IntervalList insert(const IntervalList& list, size_t offset, size_t length, NonnullRefPtr<IDataProvider> provider, size_t provider_offset);
    static Vector<HexByte> read(const IntervalList& list, size_t offset, size_t length);

private:
    static size_t append_provider_data(Vector<HexByte>& data, IDataProvider& provider, size_t offset, size_t length, bool changed);
};

class Intervals {
public:
    Intervals(Interval&& base_interval);

    const IntervalList& current() const { return m_history[m_history_index].version; }
    size_t size() const { return m_size; }

    void undo();
    void redo();

    Vector<HexByte> read(size_t offset, size_t length);
    void add_change(NonnullOwnPtr<IntervalChange> change);
    HexByte data(size_t position) const;

private:
    struct HistoryEntry {
        OwnPtr<IntervalChange> change;
        IntervalList version;
    };

    void update_length();

    Vector<HistoryEntry> m_history;
    size_t m_history_index { 0 };
    size_t m_size;
};
