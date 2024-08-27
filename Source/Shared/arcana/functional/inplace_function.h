// https://github.com/WG21-SG14/SG14/blob/master/SG14/inplace_function.h
/*
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <functional>
#include <type_traits>
#include <cassert>
#include <cstddef>

namespace stdext
{
    namespace internals
    {
        constexpr size_t InplaceFunctionDefaultCapacity = 32;

        struct inplace_function_operation
        {
            enum class operations_enum
            {
                Destroy,
                Copy,
                Move
            };
        };

        template<typename FunctorT, bool SupportsCopy>
        struct manage_function
        {
            using Operation = inplace_function_operation::operations_enum;
            static void call(void* dataPtr, const void* fromPtr, Operation op)
            {
                FunctorT* thisFunctor = reinterpret_cast<FunctorT*>(dataPtr);
                switch (op)
                {
                    case Operation::Copy:
                    {
                        if constexpr (SupportsCopy)
                        {
                            const FunctorT* source = (const FunctorT*)const_cast<void*>(fromPtr);
                            new (thisFunctor) FunctorT(*source);
                            break;
                        }
                        else
                        {
                            assert(false && "Unexpected operation");
                        }
                    }
                    case Operation::Destroy:
                    {
                        thisFunctor->~FunctorT();
                        break;
                    }
                    case Operation::Move:
                    {
                        FunctorT* source = (FunctorT*)fromPtr;
                        new (thisFunctor) FunctorT(std::move(*source));
                        break;
                    }
                    default:
                    {
                        assert(false && "Unexpected operation");
                    }
                }
            }
        };

        template<size_t Capacity, size_t Alignment, bool Copyable, typename RetT, typename... ArgsT>
        struct inplace_function_impl
        {
            using BufferType = typename std::aligned_storage<Capacity, Alignment>::type;

            inplace_function_impl()
                : m_InvokeFctPtr(&DefaultFunction)
                , m_ManagerFctPtr(nullptr)
            {}

            // Converts to 'true' if assigned
            explicit operator bool() const throw()
            {
                return m_InvokeFctPtr != &DefaultFunction;
            }

            // Invokes the target
            // Throws std::bad_function_call if not assigned
            RetT operator()(ArgsT... args) const
            {
                return m_InvokeFctPtr(std::forward<ArgsT>(args)..., data());
            }

            void swap(inplace_function_impl& other)
            {
                BufferType tempData;
                this->move(m_Data, tempData);
                other.move(other.m_Data, m_Data);
                this->move(tempData, other.m_Data);
                std::swap(m_InvokeFctPtr, other.m_InvokeFctPtr);
                std::swap(m_ManagerFctPtr, other.m_ManagerFctPtr);
            }

            void clear()
            {
                m_InvokeFctPtr = &DefaultFunction;
                if (m_ManagerFctPtr)
                    m_ManagerFctPtr(data(), nullptr, Operation::Destroy);
                m_ManagerFctPtr = nullptr;
            }

            template<size_t OtherCapacity, size_t OtherAlignment>
            void copy(const inplace_function_impl<OtherCapacity, OtherAlignment, true, RetT, ArgsT...>& other)
            {
                static_assert(OtherCapacity <= Capacity, "Can't squeeze larger inplace_function into a smaller one");
                static_assert(Alignment % OtherAlignment == 0, "Incompatible alignments");

                if (other.m_ManagerFctPtr)
                    other.m_ManagerFctPtr(data(), other.data(), Operation::Copy);

                m_InvokeFctPtr = other.m_InvokeFctPtr;
                m_ManagerFctPtr = other.m_ManagerFctPtr;
            }

            void move(BufferType& from, BufferType& to)
            {
                if (m_ManagerFctPtr)
                    m_ManagerFctPtr(&from, &to, Operation::Move);
                else
                    to = from;
            }

            template<size_t OtherCapacity, size_t OtherAlignment, bool OtherCopyable>
            void move(inplace_function_impl<OtherCapacity, OtherAlignment, OtherCopyable, RetT, ArgsT...>&& other)
            {
                static_assert(OtherCapacity <= Capacity, "Can't squeeze larger inplace_function into a smaller one");
                static_assert(Alignment % OtherAlignment == 0, "Incompatible alignments");
                static_assert(!Copyable || OtherCopyable, "Cannot move an uncopyable inplace_function into a copyable one");

                if (other.m_ManagerFctPtr)
                    other.m_ManagerFctPtr(data(), other.data(), Operation::Move);

                m_InvokeFctPtr = other.m_InvokeFctPtr;
                m_ManagerFctPtr = other.m_ManagerFctPtr;

                other.m_InvokeFctPtr = &DefaultFunction;
                // don't reset the others management function
                // because it still needs to destroy the lambda its holding.
            }

            void* data()
            {
                return &m_Data;
            }
            const void* data() const
            {
                return &m_Data;
            }

            using CompatibleFunctionPointer = RetT (*)(ArgsT...);
            using InvokeFctPtrType = RetT (*)(ArgsT..., const void* thisPtr);
            using Operation = internals::inplace_function_operation::operations_enum;
            using ManagerFctPtrType = void (*)(void* thisPtr, const void* fromPtr, Operation);

            InvokeFctPtrType m_InvokeFctPtr;
            ManagerFctPtrType m_ManagerFctPtr;

            BufferType m_Data;

            static RetT DefaultFunction(ArgsT..., const void*)
            {
                throw std::bad_function_call();
            }

            void set(std::nullptr_t)
            {
                m_ManagerFctPtr = nullptr;
                m_InvokeFctPtr = &DefaultFunction;
            }

            // For function pointers
            void set(CompatibleFunctionPointer ptr)
            {
                // this is dodgy, and - according to standard - undefined behaviour. But it works
                // see: http://stackoverflow.com/questions/559581/casting-a-function-pointer-to-another-type
                m_ManagerFctPtr = nullptr;
                m_InvokeFctPtr = reinterpret_cast<InvokeFctPtrType>(ptr);
            }

            // Set - for functors
            // enable_if makes sure this is excluded for function references and pointers.
            template<typename FunctorArgT>
            typename std::enable_if<!std::is_pointer<FunctorArgT>::value && !std::is_function<FunctorArgT>::value>::type
            set(const FunctorArgT& ftor)
            {
                using FunctorT = typename std::remove_reference<FunctorArgT>::type;
                static_assert(sizeof(FunctorT) <= Capacity, "Functor too big to fit in the buffer");
                static_assert(Alignment % alignof(FunctorArgT) == 0, "Incompatible alignment");

                // copy functor into the mem buffer
                FunctorT* buffer = reinterpret_cast<FunctorT*>(&m_Data);
                new (buffer) FunctorT(ftor);

                // generate destructor, copy-constructor and move-constructor
                m_ManagerFctPtr = &manage_function<FunctorT, Copyable>::call;

                // generate entry call
                m_InvokeFctPtr = &invoke<FunctorT>;
            }

            // Set - for functors
            // enable_if makes sure this is excluded for function references and pointers.
            template<typename FunctorArgT>
            typename std::enable_if<!std::is_pointer<FunctorArgT>::value && !std::is_function<FunctorArgT>::value>::type
            set(FunctorArgT&& ftor)
            {
                using FunctorT = typename std::remove_reference<FunctorArgT>::type;
                static_assert(sizeof(FunctorT) <= Capacity, "Functor too big to fit in the buffer");
                static_assert(Alignment % alignof(FunctorArgT) == 0, "Incompatible alignment");

                // copy functor into the mem buffer
                FunctorT* buffer = reinterpret_cast<FunctorT*>(&m_Data);
                new (buffer) FunctorT(std::move(ftor));

                // generate destructor, copy-constructor and move-constructor
                m_ManagerFctPtr = &manage_function<FunctorT, Copyable>::call;

                // generate entry call
                m_InvokeFctPtr = &invoke<FunctorT>;
            }

            template<typename FunctorT>
            static RetT invoke(ArgsT... args, const void* dataPtr)
            {
                FunctorT* functor = (FunctorT*)const_cast<void*>(dataPtr);
                return (*functor)(std::forward<ArgsT>(args)...);
            }
        };
    }

    template<typename SignatureT,
             size_t Capacity = internals::InplaceFunctionDefaultCapacity,
             size_t Alignment = alignof(std::max_align_t),
             bool Copyable = true>
    class /*alignas(Alignment)*/ inplace_function;

    template<typename RetT, typename... ArgsT, size_t Capacity, size_t Alignment>
    class /*alignas(Alignment)*/ inplace_function<RetT(ArgsT...), Capacity, Alignment, true>
    {
    public:
        template<typename SignatureT2, std::size_t Capacity2, std::size_t Alignment2, bool Copyable2>
        friend class inplace_function;

        // TODO create free operator overloads, to handle switched arguments

        // Creates an empty inplace_function
        inplace_function() = default;

        // Destroys the inplace_function. If the stored callable is valid, it is destroyed also
        ~inplace_function()
        {
            m_impl.clear();
        }

        // Creates an implace function, copying the target of other within the internal buffer
        // If the callable is larger than the internal buffer, a compile-time error is issued
        // May throw any exception encountered by the constructor when copying the target object
        template<typename CallableT>
        inplace_function(const CallableT& c)
        {
            m_impl.set(c);
        }

        // Moves the target of an implace function, storing the callable within the internal buffer
        // If the callable is larger than the internal buffer, a compile-time error is issued
        // May throw any exception encountered by the constructor when moving the target object
        template<typename CallableT, class = typename std::enable_if<!std::is_lvalue_reference<CallableT>::value>::type>
        inplace_function(CallableT&& c)
        {
            m_impl.set(std::move(c));
        }

        // Copy construct an implace_function, storing a copy of other's target internally
        // May throw any exception encountered by the constructor when copying the target object
        inplace_function(const inplace_function& other)
        {
            m_impl.copy(other.m_impl);
        }

        // Move construct an implace_function, moving the other's target to this inplace_function's internal buffer
        // May throw any exception encountered by the constructor when moving the target object
        inplace_function(inplace_function&& other)
        {
            m_impl.move(std::move(other.m_impl));
        }

        // Allows for copying from inplace_function object of the same type, but with a smaller buffer
        // May throw any exception encountered by the constructor when copying the target object
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function(const inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment, true>& other)
        {
            m_impl.copy(other.m_impl);
        }

        // Allows for moving an inplace_function object of the same type, but with a smaller buffer
        // May throw any exception encountered by the constructor when moving the target object
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function(inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment>&& other)
        {
            m_impl.move(std::move(other.m_impl));
        }

        // Assigns a copy of other's target
        // May throw any exception encountered by the assignment operator when copying the target object
        inplace_function& operator=(const inplace_function& other)
        {
            m_impl.clear();
            m_impl.copy(other.m_impl);
            return *this;
        }

        // Assigns the other's target by way of moving
        // May throw any exception encountered by the assignment operator when moving the target object
        inplace_function& operator=(inplace_function&& other)
        {
            m_impl.clear();
            m_impl.move(std::move(other.m_impl));
            return *this;
        }

        // Allows for copy assignment of an inplace_function object of the same type, but with a smaller buffer
        // If the copy constructor of target object throws, this is left in uninitialized state
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function& operator=(const inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment, true>& other)
        {
            m_impl.clear();
            m_impl.copy(other.m_impl);
            return *this;
        }

        // Allows for move assignment of an inplace_function object of the same type, but with a smaller buffer
        // If the move constructor of target object throws, this is left in uninitialized state
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function& operator=(inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment>&& other)
        {
            m_impl.clear();
            m_impl.move(std::move(other.m_impl));
            return *this;
        }

        // Assign a new target
        // If the copy constructor of target object throws, this is left in uninitialized state
        template<typename CallableT, class = typename std::enable_if<!std::is_lvalue_reference<CallableT>::value>::type>
        inplace_function& operator=(const CallableT& target)
        {
            m_impl.clear();
            m_impl.set(target);
            return *this;
        }

        // Assign a new target by way of moving
        // If the move constructor of target object throws, this is left in uninitialized state
        template<typename Callable>
        inplace_function& operator=(Callable&& target)
        {
            m_impl.clear();
            m_impl.set(std::move(target));
            return *this;
        }

        // Compares this inplace function with a null pointer
        // Empty functions compare equal, non-empty functions compare unequal
        bool operator==(std::nullptr_t)
        {
            return !operator bool();
        }

        // Compares this inplace function with a null pointer
        // Empty functions compare equal, non-empty functions compare unequal
        bool operator!=(std::nullptr_t)
        {
            return operator bool();
        }

        // Converts to 'true' if assigned
        explicit operator bool() const throw()
        {
            return m_impl.operator bool();
        }

        // Invokes the target
        // Throws std::bad_function_call if not assigned
        RetT operator()(ArgsT... args) const
        {
            return m_impl(std::forward<ArgsT>(args)...);
        }

        // Swaps two targets
        void swap(inplace_function& other)
        {
            m_impl.swap(other.m_impl);
        }

    private:
        using ImplT = internals::inplace_function_impl<Capacity, Alignment, true, RetT, ArgsT...>;
        ImplT m_impl{};
    };

    template<typename RetT, typename... ArgsT, size_t Capacity, size_t Alignment>
    class /*alignas(Alignment)*/ inplace_function<RetT(ArgsT...), Capacity, Alignment, false>
    {
    public:
        template<typename SignatureT2, std::size_t Capacity2, std::size_t Alignment2, bool Copyable2>
        friend class inplace_function;

        // TODO create free operator overloads, to handle switched arguments

        // Creates an empty inplace_function
        inplace_function() = default;

        // Destroys the inplace_function. If the stored callable is valid, it is destroyed also
        ~inplace_function()
        {
            m_impl.clear();
        }

        // Creates an implace function, copying the target of other within the internal buffer
        // If the callable is larger than the internal buffer, a compile-time error is issued
        // May throw any exception encountered by the constructor when copying the target object
        template<typename CallableT>
        inplace_function(const CallableT& c)
        {
            m_impl.set(c);
        }

        // Moves the target of an implace function, storing the callable within the internal buffer
        // If the callable is larger than the internal buffer, a compile-time error is issued
        // May throw any exception encountered by the constructor when moving the target object
        template<typename CallableT, class = typename std::enable_if<!std::is_lvalue_reference<CallableT>::value>::type>
        inplace_function(CallableT&& c)
        {
            m_impl.set(std::move(c));
        }

        // Copy construction is not possible from uncopyable inplace_functions.
        inplace_function(const inplace_function& other) = delete;

        // Move construct an implace_function, moving the other�s target to this inplace_function�s internal buffer
        // May throw any exception encountered by the constructor when moving the target object
        inplace_function(inplace_function&& other)
        {
            m_impl.move(std::move(other.m_impl));
        }

        // Allows for copying from inplace_function object of the same type, but with a smaller buffer
        // May throw any exception encountered by the constructor when copying the target object
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function(const inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment, true>& other)
        {
            m_impl.copy(other.m_impl);
        }

        // Allows for moving an inplace_function object of the same type, but with a smaller buffer
        // May throw any exception encountered by the constructor when moving the target object
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function(inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment>&& other)
        {
            m_impl.move(std::move(other.m_impl));
        }

        // Copy assignment is not possible from uncopyable inplace_functions
        inplace_function& operator=(const inplace_function& other) = delete;

        // Assigns the other�s target by way of moving
        // May throw any exception encountered by the assignment operator when moving the target object
        inplace_function& operator=(inplace_function&& other)
        {
            m_impl.clear();
            m_impl.move(std::move(other.m_impl));
            return *this;
        }

        // Allows for copy assignment of an inplace_function object of the same type, but with a smaller buffer
        // If the copy constructor of target object throws, this is left in uninitialized state
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function& operator=(const inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment, true>& other)
        {
            m_impl.clear();
            m_impl.copy(other);
            return *this;
        }

        // Allows for move assignment of an inplace_function object of the same type, but with a smaller buffer
        // If the move constructor of target object throws, this is left in uninitialized state
        // If OtherCapacity is greater than Capacity, a compile-time error is issued
        template<size_t OtherCapacity, size_t OtherAlignment>
        inplace_function& operator=(inplace_function<RetT(ArgsT...), OtherCapacity, OtherAlignment>&& other)
        {
            m_impl.clear();
            m_impl.move(std::move(other));
            return *this;
        }

        // Assign a new target
        // If the copy constructor of target object throws, this is left in uninitialized state
        template<typename CallableT, class = typename std::enable_if<!std::is_lvalue_reference<CallableT>::value>::type>
        inplace_function& operator=(const CallableT& target)
        {
            m_impl.clear();
            m_impl.set(target);
            return *this;
        }

        // Assign a new target by way of moving
        // If the move constructor of target object throws, this is left in uninitialized state
        template<typename Callable>
        inplace_function& operator=(Callable&& target)
        {
            m_impl.clear();
            m_impl.set(std::move(target));
            return *this;
        }

        // Compares this inplace function with a null pointer
        // Empty functions compare equal, non-empty functions compare unequal
        bool operator==(std::nullptr_t)
        {
            return !operator bool();
        }

        // Compares this inplace function with a null pointer
        // Empty functions compare equal, non-empty functions compare unequal
        bool operator!=(std::nullptr_t)
        {
            return operator bool();
        }

        // Converts to 'true' if assigned
        explicit operator bool() const throw()
        {
            return m_impl.operator bool();
        }

        // Invokes the target
        // Throws std::bad_function_call if not assigned
        RetT operator()(ArgsT... args) const
        {
            return m_impl(std::forward<ArgsT>(args)...);
        }

        // Swaps two targets
        void swap(inplace_function& other)
        {
            m_impl.swap(other.m_impl);
        }

    private:
        using ImplT = internals::inplace_function_impl<Capacity, Alignment, false, RetT, ArgsT...>;
        ImplT m_impl{};
    };
}
