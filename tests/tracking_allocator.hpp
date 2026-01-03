#ifndef RPNX_TESTS_TRACKING_ALLOCATOR_HPP
#define RPNX_TESTS_TRACKING_ALLOCATOR_HPP

#include <memory>

namespace testutils
{
    template <typename T>
    struct tracking_allocator
    {
        using value_type = T;
        int id = 0;

        tracking_allocator(int id = 0) : id(id)
        {
        }

        template <typename U>
        tracking_allocator(const tracking_allocator<U>& other) : id(other.id)
        {
        }

        T* allocate(std::size_t n)
        {
            return std::allocator<T>().allocate(n);
        }

        void deallocate(T* p, std::size_t n)
        {
            std::allocator<T>().deallocate(p, n);
        }

        bool operator==(const tracking_allocator& other) const
        {
            return id == other.id;
        }

        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;

        tracking_allocator select_on_container_copy_construction() const
        {
            return tracking_allocator(id + 100);
        }
    };

    template <typename T>
    struct non_propagate_allocator
    {
        using value_type = T;
        int id;

        non_propagate_allocator(int id = 0) : id(id)
        {
        }

        template <typename U>
        non_propagate_allocator(const non_propagate_allocator<U>& other) : id(other.id)
        {
        }

        T* allocate(std::size_t n)
        {
            return std::allocator<T>().allocate(n);
        }

        void deallocate(T* p, std::size_t n)
        {
            std::allocator<T>().deallocate(p, n);
        }

        bool operator==(const non_propagate_allocator& other) const
        {
            return id == other.id;
        }

        using propagate_on_container_copy_assignment = std::false_type;
    };
} // namespace testutils

#endif // RPNX_TESTS_TRACKING_ALLOCATOR_HPP
