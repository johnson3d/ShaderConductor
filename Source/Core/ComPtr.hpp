/*
 * ShaderConductor
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cassert>
#include <type_traits>

#include <dxc/WinAdapter.h>

#include "ErrorHandling.hpp"

namespace ShaderConductor
{
    template <typename T>
    class ComPtr
    {
        template <typename U>
        friend class ComPtr;

    public:
        using element_type = std::remove_extent_t<T>;

    public:
        ComPtr() noexcept = default;

        ComPtr(std::nullptr_t) noexcept
        {
        }

        explicit ComPtr(T* ptr) noexcept : ptr_(ptr)
        {
            this->InternalAddRef();
        }

        template <typename U>
        explicit ComPtr(U* ptr) noexcept : ptr_(ptr)
        {
            this->InternalAddRef();
        }

        ComPtr(const ComPtr& rhs) noexcept : ComPtr(rhs.ptr_, true)
        {
        }

        template <typename U>
        ComPtr(const ComPtr<U>& rhs) noexcept : ComPtr(rhs.ptr_, true)
        {
        }

        ComPtr(ComPtr&& rhs) noexcept : ptr_(std::exchange(rhs.ptr_, {}))
        {
        }

        template <typename U, std::enable_if_t<!std::is_same_v<T, U>, bool> = true>
        ComPtr(ComPtr<U>&& rhs) noexcept : ptr_(std::exchange(rhs.ptr_, {}))
        {
        }

        ~ComPtr() noexcept
        {
            this->InternalRelease();
        }

        ComPtr& operator=(const ComPtr& rhs) noexcept
        {
            if (ptr_ != rhs.ptr_)
            {
                this->InternalRelease();
                ptr_ = rhs.ptr_;
                this->InternalAddRef();
            }
            return *this;
        }

        ComPtr& operator=(ComPtr&& rhs) noexcept
        {
            if (ptr_ != rhs.ptr_)
            {
                this->InternalRelease();
                ptr_ = std::exchange(rhs.ptr_, {});
            }
            return *this;
        }

        template <typename U>
        ComPtr& operator=(const ComPtr<U>& rhs) noexcept
        {
            if (ptr_ != rhs.ptr_)
            {
                this->InternalRelease();
                ptr_ = rhs.ptr_;
                this->InternalAddRef();
            }
            return *this;
        }

        template <typename U, std::enable_if_t<!std::is_same_v<T, U>, bool> = true>
        ComPtr& operator=(ComPtr<U>&& rhs) noexcept
        {
            this->InternalRelease();
            ptr_ = std::exchange(rhs.ptr_, {});
            return *this;
        }

        explicit operator bool() const noexcept
        {
            return (ptr_ != nullptr);
        }

        T& operator*() const noexcept
        {
            return *ptr_;
        }

        T* operator->() const noexcept
        {
            return ptr_;
        }

        T* Get() const noexcept
        {
            return ptr_;
        }

        T** Put() noexcept
        {
            assert(ptr_ == nullptr);
            return &ptr_;
        }

        void** PutVoid() noexcept
        {
            return reinterpret_cast<void**>(this->Put());
        }

        T* Detach() noexcept
        {
            return std::exchange(ptr_, {});
        }

        void Reset() noexcept
        {
            this->InternalRelease();
        }

        template <typename U>
        ComPtr<U> As() const
        {
            ComPtr<U> ret;
            TIFHR(ptr_->QueryInterface(__uuidof(U), ret.PutVoid()));
            return ret;
        }

    private:
        void InternalAddRef() noexcept
        {
            if (ptr_ != nullptr)
            {
                ptr_->AddRef();
            }
        }

        void InternalRelease() noexcept
        {
            if (ptr_ != nullptr)
            {
                std::exchange(ptr_, {})->Release();
            }
        }

    private:
        T* ptr_ = nullptr;
    };

    template <typename T, typename U>
    bool operator==(const ComPtr<T>& lhs, const ComPtr<U>& rhs) noexcept
    {
        return lhs.Get() == rhs.Get();
    }

    template <typename T>
    bool operator==(const ComPtr<T>& lhs, [[maybe_unused]] std::nullptr_t rhs) noexcept
    {
        return lhs.Get() == nullptr;
    }

    template <typename T>
    bool operator==(std::nullptr_t lhs, const ComPtr<T>& rhs) noexcept
    {
        return rhs == lhs;
    }

    template <typename T, typename U>
    bool operator!=(const ComPtr<T>& lhs, const ComPtr<U>& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template <typename T>
    bool operator!=(const ComPtr<T>& lhs, std::nullptr_t rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template <typename T>
    bool operator!=(std::nullptr_t lhs, const ComPtr<T>& rhs) noexcept
    {
        return !(lhs == rhs);
    }
} // namespace ShaderConductor
