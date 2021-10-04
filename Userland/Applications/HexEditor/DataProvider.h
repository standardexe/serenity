/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Arne Elster <arne@elster.li>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/StringView.h>
#include <AK/Types.h>
#include <LibCore/File.h>

class IDataProvider : public RefCounted<IDataProvider> {
public:
    virtual ~IDataProvider() = default;

    virtual u8 data(const size_t offset) = 0;
    virtual size_t size() const = 0;
};

class DataProviderMemory : public IDataProvider {
public:
    DataProviderMemory(ByteBuffer&& buffer)
        : m_buffer(std::move(buffer))
    {
    }

    u8 data(const size_t offset) override
    {
        return m_buffer[offset];
    }

    size_t size() const override { return m_buffer.size(); }

private:
    const ByteBuffer m_buffer;
};

class DataProviderFileSystem : public IDataProvider {
public:
    DataProviderFileSystem(NonnullRefPtr<Core::File> fd)
        : m_file(fd)
    {
        struct stat file_stat;
        if (lstat(fd->filename().characters(), &file_stat) < 0) {
            m_file_size = 0;
        } else {
            m_file_size = file_stat.st_size;
        }
    }

    u8 data(const size_t offset) override
    {
        if (offset < m_buffer_offset || offset >= m_buffer_offset + m_buffer.size()) {
            m_file->seek(offset, Core::SeekMode::SetPosition);
            m_buffer = m_file->read(64 * KiB);
            m_buffer_offset = offset;
        }
        return m_buffer[offset - m_buffer_offset];
    }

    size_t size() const override { return m_file_size; }

private:
    NonnullRefPtr<Core::File> m_file;
    ByteBuffer m_buffer;
    size_t m_buffer_offset;
    size_t m_file_size;
};
