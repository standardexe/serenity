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
    bool saved;
};

class VersionPart {
public:
    VersionPart(size_t size, size_t provider_offset, NonnullRefPtr<IDataProvider> provider, bool saved = false)
        : m_size(size)
        , m_data_provider(provider)
        , m_data_provider_offset(provider_offset)
        , m_saved(saved)
    {
    }

    size_t size() const { return m_size; }
    NonnullRefPtr<IDataProvider> provider() const { return m_data_provider; }
    size_t provider_offset() const { return m_data_provider_offset; }
    bool saved() const { return m_saved; }

private:
    size_t m_size;
    NonnullRefPtr<IDataProvider> m_data_provider;
    size_t m_data_provider_offset;
    bool m_saved;
};

using Version = Vector<VersionPart>;

class VersionChange {
public:
    enum class ChangeType {
        Insert,
        Delete,
        Modify
    };

    VersionChange(ChangeType type, size_t offset, size_t size)
        : m_type(type)
        , m_offset(offset)
        , m_size(size)
    {
    }

    virtual ~VersionChange() = default;

    ChangeType type() const { return m_type; }
    size_t offset() const { return m_offset; }
    size_t size() const { return m_size; }

    virtual Optional<Version> apply(const Version& version) = 0;
    virtual Optional<Version> revert(const Version& version) = 0;

private:
    ChangeType m_type;
    size_t m_offset;
    size_t m_size;
};

class VersionChangeModify;
class VersionChangeInsert;
class VersionChangeRemove;

class VersionChangeModify : public VersionChange {
public:
    VersionChangeModify(NonnullRefPtr<IDataProvider> provider, size_t provider_offset, size_t offset, size_t size);
    virtual ~VersionChangeModify() = default;

    NonnullRefPtr<IDataProvider> provider() const { return m_provider; }
    size_t provider_offset() const { return m_provider_offset; }

    Optional<Version> apply(const Version& version) override;
    Optional<Version> revert(const Version& version) override;

private:
    NonnullRefPtr<IDataProvider> m_provider;
    size_t m_provider_offset;

    NonnullOwnPtr<VersionChangeInsert> m_change_insert;
    NonnullOwnPtr<VersionChangeRemove> m_change_remove;
};

class VersionChangeInsert : public VersionChange {
public:
    VersionChangeInsert(NonnullRefPtr<IDataProvider> provider, size_t provider_offset, size_t offset, size_t size)
        : VersionChange(ChangeType::Insert, offset, size)
        , m_provider(provider)
        , m_provider_offset(provider_offset)
    {
    }

    virtual ~VersionChangeInsert() = default;

    NonnullRefPtr<IDataProvider> provider() const { return m_provider; }
    size_t provider_offset() const { return m_provider_offset; }

    Optional<Version> apply(const Version& version) override;
    Optional<Version> revert(const Version& version) override;

private:
    NonnullRefPtr<IDataProvider> m_provider;
    size_t m_provider_offset;
};

class VersionChangeRemove : public VersionChange {
public:
    VersionChangeRemove(size_t offset, size_t size)
        : VersionChange(ChangeType::Delete, offset, size)
    {
    }

    virtual ~VersionChangeRemove() = default;

    Optional<Version> apply(const Version& version) override;
    Optional<Version> revert(const Version& version) override;

private:
    RefPtr<IDataProvider> m_deleted_data_provider;
};

class VersionTools {
public:
    static Version from_array(ByteBuffer&& buffer);
    static Version remove(const Version& list, size_t offset, size_t length);
    static Version insert(const Version& list, size_t offset, size_t length, NonnullRefPtr<IDataProvider> provider, size_t provider_offset);
    static Vector<HexByte> read(const Version& list, size_t offset, size_t length);

private:
    static size_t append_provider_data(Vector<HexByte>& data, IDataProvider& provider, size_t offset, size_t length, bool changed);
};

class VersionHistory {
public:
    VersionHistory(VersionPart&& base_version);

    const Version& current() const { return m_history[m_history_index].version; }
    size_t size() const { return m_size; }

    void undo();
    void redo();

    Vector<HexByte> read(size_t offset, size_t length);
    bool add_change(NonnullOwnPtr<VersionChange> change);
    HexByte data(size_t position) const;
    bool set_new_base_version_as_current_version(VersionPart&& base_version);

private:
    struct HistoryEntry {
        OwnPtr<VersionChange> change;
        Version version;
    };

    void update_length();

    Vector<HistoryEntry> m_history;
    size_t m_history_index { 0 };
    size_t m_base_index { 0 };
    size_t m_size { 0 };
};
