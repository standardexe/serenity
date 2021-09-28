/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Arne Elster <arne@elster.li>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Intervals.h"

IntervalChangeModify::IntervalChangeModify(NonnullRefPtr<IDataProvider> provider, size_t provider_offset, size_t offset, size_t size)
    : IntervalChange(ChangeType::Modify, offset, size)
    , m_provider(provider)
    , m_provider_offset(provider_offset)
{
    m_change_insert = make<IntervalChangeInsert>(provider, provider_offset, offset, size);
    m_change_remove = make<IntervalChangeRemove>(offset, size);
}

IntervalList IntervalChangeModify::apply(const IntervalList& intervals)
{
    return m_change_insert->apply(m_change_remove->apply(intervals));
}

IntervalList IntervalChangeInsert::apply(const IntervalList& intervals)
{
    return IntervalTools::insert(intervals, offset(), size(), provider(), provider_offset());
}

IntervalList IntervalChangeRemove::apply(const IntervalList& intervals)
{
    return IntervalTools::remove(intervals, offset(), size());
}

Intervals::Intervals(Interval&& base_interval)
{
    m_history.append(HistoryEntry { OwnPtr<IntervalChange>(nullptr), IntervalList { base_interval } });
    update_length();
}

void Intervals::undo()
{
    m_history_index = m_history_index == 0 ? 0 : m_history_index - 1;
    update_length();
}

void Intervals::redo()
{
    m_history_index = min(m_history.size() - 1, m_history_index + 1);
    update_length();
}

Vector<HexByte> Intervals::read(size_t offset, size_t length)
{
    return IntervalTools::read(current(), offset, length);
}

void Intervals::add_change(NonnullOwnPtr<IntervalChange> change)
{
    auto new_interval = change->apply(current());
    m_history.remove(m_history_index + 1, m_history.size() - m_history_index - 1);
    m_history.append(HistoryEntry { move(change), new_interval });
    ++m_history_index;
    update_length();
}

HexByte Intervals::data(size_t position) const
{
    return IntervalTools::read(current(), position, 1).first();
}

void Intervals::update_length()
{
    m_size = 0;
    for (size_t i = 0; i < current().size(); i++) {
        m_size += current()[i].size();
    }
}

IntervalList IntervalTools::from_array(ByteBuffer&& buffer)
{
    Interval interval { buffer.size(), 0, adopt_ref(*new DataProviderMemory(move(buffer))) };
    return { interval };
}

IntervalList IntervalTools::remove(const IntervalList& list, size_t offset, size_t length)
{
    IntervalList result;
    size_t count = 0;

    for (size_t i = 0; i < list.size(); i++) {
        if (offset >= count && offset < count + list[i].size()) {
            if (length <= list[i].size() - (offset - count)) {
                // 1. case: length smaller than list[i].length
                Interval segmentBefore { offset - count, list[i].provider_offset(), list[i].provider(), list[i].changed() };
                Interval segmentAfter { list[i].size() - segmentBefore.size() - length, list[i].provider_offset() + segmentBefore.size() + length, list[i].provider(), list[i].changed() };
                if (segmentBefore.size() > 0)
                    result.append(move(segmentBefore));
                if (segmentAfter.size() > 0)
                    result.append(move(segmentAfter));

                for (i++; i < list.size(); i++) {
                    result.append(Interval(list[i]));
                }
                return result;
            } else {
                // 2. case: length greater than list[i].length
                Interval segmentBefore { offset - count, list[i].provider_offset(), list[i].provider(), list[i].changed() };
                result.append(move(segmentBefore));
                length -= list[i].size() - (offset - count);
                i++;

                while (length > 0 && i < list.size()) {
                    if (length > list[i].size()) {
                        length -= list[i].size();
                        i++;
                    } else {
                        Interval segmentAfter { list[i].size() - length, list[i].provider_offset() + length, list[i].provider(), list[i].changed() };
                        result.append(move(segmentAfter));
                        for (i++; i < list.size(); i++) {
                            result.append(Interval(list[i]));
                        }
                        return result;
                    }
                }
                VERIFY_NOT_REACHED(); // Segment to delete too long
            }
        } else {
            result.append(Interval(list[i]));
            count += list[i].size();
        }
    }

    VERIFY_NOT_REACHED(); // Invalid position to delete
}

IntervalList IntervalTools::insert(const IntervalList& list, size_t offset, size_t length, NonnullRefPtr<IDataProvider> provider, size_t provider_offset)
{
    if (length == 0) {
        return IntervalList(list);
    }

    size_t count = 0;
    bool added = false;
    IntervalList result;
    Interval interval_new { length, provider_offset, provider };

    for (size_t i = 0; i < list.size(); i++) {
        if (offset == count && !added) {
            result.append(move(interval_new));
            result.append(Interval(list[i]));
            added = true;
        } else if ((offset > count && offset < count + list[i].size()) && !added) {
            Interval interval_before { offset - count, list[i].provider_offset(), list[i].provider(), list[i].changed() };
            Interval interval_after { list[i].size() - (offset - count), list[i].provider_offset() + (offset - count), list[i].provider(), list[i].changed() };
            result.append(move(interval_before));
            result.append(move(interval_new));
            result.append(move(interval_after));
            added = true;
        } else {
            result.append(Interval(list[i]));
        }

        count += list[i].size();
    }

    if (!added && count == offset) {
        result.append(move(interval_new));
        added = true;
    }

    VERIFY(added);

    return result;
}

Vector<HexByte> IntervalTools::read(const IntervalList& list, size_t offset, size_t length)
{
    size_t count = 0;
    size_t read_total = 0;

    Vector<HexByte> result;

    for (size_t i = 0; i < list.size(); i++) {
        if (offset >= count && offset <= count + list[i].size()) {
            if ((offset - count) + length < list[i].size()) {
                auto provider_offset = list[i].provider_offset() + (offset - count);
                append_provider_data(result, *list[i].provider(), provider_offset, length, list[i].changed());
            } else {
                auto provider_offset = list[i].provider_offset() + (offset - count);
                auto should_read = list[i].size() - (offset - count);
                auto actual_read = append_provider_data(result, *list[i].provider(), provider_offset, should_read, list[i].changed());
                length -= actual_read;
                read_total += actual_read;
                ++i;

                while (length > 0 && i < list.size()) {
                    should_read = max(length, list[i].size());
                    actual_read = append_provider_data(result, *list[i].provider(), list[i].provider_offset(), should_read, list[i].changed());
                    length -= actual_read;
                    read_total += actual_read;
                    ++i;
                }

                break;
            }
        }
        count += list[i].size();
    }

    return result;
}

size_t IntervalTools::append_provider_data(Vector<HexByte>& data, IDataProvider& provider, size_t offset, size_t length, bool changed)
{
    size_t total_read = 0;
    for (size_t i = 0; (i < length) && (offset + i < provider.size()); i++) {
        data.append({ provider.data(offset + i), changed });
        ++total_read;
    }
    return total_read;
}
