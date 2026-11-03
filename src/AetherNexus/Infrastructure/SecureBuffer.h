// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: SecureBuffer.h
// Project: AetherNexus
#pragma once
#include <windows.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace AetherNexus::Infrastructure
{
// Fixed allocation: no small-string optimization, growth, or implicit plaintext copies.
class SecureBuffer final
{
  public:
    explicit SecureBuffer(size_t capacity = 0)
        : m_data(capacity ? std::make_unique<char[]>(capacity) : nullptr), m_capacity(capacity)
    {
    }
    ~SecureBuffer()
    {
        Clear();
    }
    SecureBuffer(SecureBuffer const &) = delete;
    SecureBuffer &operator=(SecureBuffer const &) = delete;
    SecureBuffer(SecureBuffer &&other) noexcept
    {
        *this = std::move(other);
    }
    SecureBuffer &operator=(SecureBuffer &&other) noexcept
    {
        if (this != &other)
        {
            Clear();
            m_data = std::move(other.m_data);
            m_capacity = std::exchange(other.m_capacity, 0);
            m_size = std::exchange(other.m_size, 0);
        }
        return *this;
    }
    char *Data() noexcept
    {
        return m_data.get();
    }
    size_t Capacity() const noexcept
    {
        return m_capacity;
    }
    std::string_view View() const noexcept
    {
        return {m_data.get(), m_size};
    }
    void Append(std::string_view value)
    {
        if (value.size() > m_capacity - m_size)
            throw std::length_error("Sensitive buffer limit exceeded");
        if (!value.empty())
            std::memcpy(m_data.get() + m_size, value.data(), value.size());
        m_size += value.size();
    }
    void Clear() noexcept
    {
        if (m_data)
            SecureZeroMemory(m_data.get(), m_capacity);
        m_size = 0;
    }

  private:
    std::unique_ptr<char[]> m_data;
    size_t m_capacity{}, m_size{};
};
} // namespace AetherNexus::Infrastructure
