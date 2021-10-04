/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Arne Elster <arne@elster.li>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Versions.h"

VersionChangeModify::VersionChangeModify(NonnullRefPtr<IDataProvider> provider, size_t provider_offset, size_t offset, size_t size)
    : VersionChange(ChangeType::Modify, offset, size)
    , m_provider(provider)
    , m_provider_offset(provider_offset)
    , m_change_insert(make<VersionChangeInsert>(provider, provider_offset, offset, size))
    , m_change_remove(make<VersionChangeRemove>(offset, size))
{
}

Optional<Version> VersionChangeModify::apply(const Version& version)
{
    auto applied_remove = m_change_remove->apply(version);
    if (!applied_remove.has_value())
        return {};
    return m_change_insert->apply(applied_remove.value());
}

Optional<Version> VersionChangeModify::revert(const Version& version)
{
    auto applied_insert = m_change_insert->revert(version);
    if (!applied_insert.has_value())
        return {};
    return m_change_remove->revert(applied_insert.value());
}

Optional<Version> VersionChangeInsert::apply(const Version& version)
{
    return VersionTools::insert(version, offset(), size(), provider(), provider_offset());
}

Optional<Version> VersionChangeInsert::revert(const Version& version)
{
    return VersionTools::remove(version, offset(), size());
}

Optional<Version> VersionChangeRemove::apply(const Version& version)
{
    Vector<HexByte> hex_buffer = VersionTools::read(version, offset(), size());
    auto maybe_byte_buffer = ByteBuffer::create_uninitialized(hex_buffer.size());

    if (!maybe_byte_buffer.has_value()) {
        m_deleted_data_provider.release_nonnull();
        return {};
    }

    ByteBuffer byte_buffer = maybe_byte_buffer.release_value();
    for (size_t i = 0; i < hex_buffer.size(); i++) {
        byte_buffer.data()[i] = hex_buffer[i].value;
    }
    m_deleted_data_provider = RefPtr<IDataProvider>(*new DataProviderMemory(move(byte_buffer)));

    return VersionTools::remove(version, offset(), size());
}

Optional<Version> VersionChangeRemove::revert(const Version& version)
{
    return VersionTools::insert(version, offset(), size(), NonnullRefPtr<IDataProvider>(*m_deleted_data_provider.ptr()), 0);
}

VersionHistory::VersionHistory(VersionPart&& base_version)
{
    m_history.append(HistoryEntry { OwnPtr<VersionChange>(nullptr), Version { base_version } });
    update_length();
}

void VersionHistory::undo()
{
    m_history_index = m_history_index == 0 ? 0 : m_history_index - 1;
    update_length();
}

void VersionHistory::redo()
{
    m_history_index = min(m_history.size() - 1, m_history_index + 1);
    update_length();
}

Vector<HexByte> VersionHistory::read(size_t offset, size_t length)
{
    return VersionTools::read(current(), offset, length);
}

bool VersionHistory::add_change(NonnullOwnPtr<VersionChange> change)
{
    auto new_version = change->apply(current());
    if (!new_version.has_value())
        return false;
    m_history.remove(m_history_index + 1, m_history.size() - m_history_index - 1);
    m_history.append(HistoryEntry { move(change), new_version.value() });
    ++m_history_index;
    update_length();
    return true;
}

HexByte VersionHistory::data(size_t position) const
{
    return VersionTools::read(current(), position, 1).first();
}

bool VersionHistory::set_new_base_version_as_current_version(VersionPart&& base_version)
{
    if (m_base_index < m_history_index) {
        m_history.insert(m_history_index + 1, HistoryEntry { nullptr, { base_version } });
        m_history.remove(m_base_index);
        m_base_index = m_history_index;
    } else {
        m_history.insert(m_history_index, HistoryEntry { nullptr, { base_version } });
        m_history.remove(m_base_index + 1);
        m_base_index = m_history_index;
    }

    for (int i = m_base_index - 1; i >= 0; i--) {
        if (m_history[i].change.ptr()) {
            auto change = m_history[i].change->revert(m_history[i + 1].version);
            if (!change.has_value())
                return false;
            m_history[i].version = change.value();
        }
    }

    for (size_t i = m_base_index + 1; i < m_history.size(); i++) {
        if (m_history[i].change.ptr()) {
            auto change = m_history[i].change->apply(m_history[i - 1].version);
            if (!change.has_value())
                return false;
            m_history[i].version = change.value();
        }
    }

    return true;
}

void VersionHistory::update_length()
{
    m_size = 0;
    for (size_t i = 0; i < current().size(); i++) {
        m_size += current()[i].size();
    }
}

Version VersionTools::from_array(ByteBuffer&& buffer)
{
    VersionPart version_part { buffer.size(), 0, adopt_ref(*new DataProviderMemory(move(buffer))) };
    return { version_part };
}

Version VersionTools::remove(const Version& version, size_t offset, size_t length)
{
    Version result;
    size_t count = 0;

    for (size_t i = 0; i < version.size(); i++) {
        if (offset >= count && offset < count + version[i].size()) {
            if (length <= version[i].size() - (offset - count)) {
                // 1. case: length smaller than version[i].length
                VersionPart segmentBefore { offset - count, version[i].provider_offset(), version[i].provider(), version[i].saved() };
                VersionPart segmentAfter { version[i].size() - segmentBefore.size() - length, version[i].provider_offset() + segmentBefore.size() + length, version[i].provider(), version[i].saved() };
                if (segmentBefore.size() > 0)
                    result.append(move(segmentBefore));
                if (segmentAfter.size() > 0)
                    result.append(move(segmentAfter));

                for (i++; i < version.size(); i++) {
                    result.append(VersionPart(version[i]));
                }
                return result;
            } else {
                // 2. case: length greater than version[i].length
                VersionPart segmentBefore { offset - count, version[i].provider_offset(), version[i].provider(), version[i].saved() };
                result.append(move(segmentBefore));
                length -= version[i].size() - (offset - count);
                i++;

                while (length > 0 && i < version.size()) {
                    if (length > version[i].size()) {
                        length -= version[i].size();
                        i++;
                    } else {
                        VersionPart segmentAfter { version[i].size() - length, version[i].provider_offset() + length, version[i].provider(), version[i].saved() };
                        result.append(move(segmentAfter));
                        for (i++; i < version.size(); i++) {
                            result.append(VersionPart(version[i]));
                        }
                        return result;
                    }
                }
                VERIFY_NOT_REACHED(); // Segment to delete too long
            }
        } else {
            result.append(VersionPart(version[i]));
            count += version[i].size();
        }
    }

    VERIFY_NOT_REACHED(); // Invalid position to delete
}

Version VersionTools::insert(const Version& version, size_t offset, size_t length, NonnullRefPtr<IDataProvider> provider, size_t provider_offset)
{
    if (length == 0) {
        return Version(version);
    }

    size_t count = 0;
    bool added = false;
    Version result;
    VersionPart version_part_new { length, provider_offset, provider };

    for (size_t i = 0; i < version.size(); i++) {
        if (offset == count && !added) {
            result.append(move(version_part_new));
            result.append(VersionPart(version[i]));
            added = true;
        } else if ((offset > count && offset < count + version[i].size()) && !added) {
            VersionPart version_part_before { offset - count, version[i].provider_offset(), version[i].provider(), version[i].saved() };
            VersionPart version_part_after { version[i].size() - (offset - count), version[i].provider_offset() + (offset - count), version[i].provider(), version[i].saved() };
            result.append(move(version_part_before));
            result.append(move(version_part_new));
            result.append(move(version_part_after));
            added = true;
        } else {
            result.append(VersionPart(version[i]));
        }

        count += version[i].size();
    }

    if (!added && count == offset) {
        result.append(move(version_part_new));
        added = true;
    }

    VERIFY(added);

    return result;
}

Vector<HexByte> VersionTools::read(const Version& version, size_t offset, size_t length)
{
    size_t count = 0;
    size_t read_total = 0;

    Vector<HexByte> result;

    for (size_t i = 0; i < version.size(); i++) {
        if (offset >= count && offset <= count + version[i].size()) {
            if ((offset - count) + length < version[i].size()) {
                auto provider_offset = version[i].provider_offset() + (offset - count);
                append_provider_data(result, *version[i].provider(), provider_offset, length, version[i].saved());
            } else {
                auto provider_offset = version[i].provider_offset() + (offset - count);
                auto should_read = version[i].size() - (offset - count);
                auto actual_read = append_provider_data(result, *version[i].provider(), provider_offset, should_read, version[i].saved());
                length -= actual_read;
                read_total += actual_read;
                ++i;

                while (length > 0 && i < version.size()) {
                    should_read = max(length, version[i].size());
                    actual_read = append_provider_data(result, *version[i].provider(), version[i].provider_offset(), should_read, version[i].saved());
                    length -= actual_read;
                    read_total += actual_read;
                    ++i;
                }

                break;
            }
        }
        count += version[i].size();
    }

    return result;
}

size_t VersionTools::append_provider_data(Vector<HexByte>& data, IDataProvider& provider, size_t offset, size_t length, bool saved)
{
    size_t total_read = 0;
    for (size_t i = 0; (i < length) && (offset + i < provider.size()); i++) {
        data.append({ provider.data(offset + i), saved });
        ++total_read;
    }
    return total_read;
}
