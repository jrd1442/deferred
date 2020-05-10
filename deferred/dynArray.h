#pragma once
#include <memory>
#include "util.h"

template <class T>
struct DynArray
{
    DynArray() : capacity(16), size(0)
    {
        arr = std::make_unique<T[]>(capacity);
    };

    ~DynArray() = default;

    T& operator[] (const uint idx) const
    {
        assert(idx < capacity);
        return arr[idx];
    }

    void push(T val) // allows for more flexibility at the call site
    {
        if (size == capacity)
        {
            // resize
            capacity *= 2;
            std::unique_ptr<T[]> tarr = std::make_unique<T[]>(capacity); // nitpick: 'auto tarr =' because we can tell it's a unique_ptr
            std::move(std::begin(arr), std::end(arr), std::begin(tarr));
            arr = std::move(tarr);
        }
        assert(size < capacity);
        arr[size] = std::move(val);
        size++;
    }

    void erase()
    {
        if (arr != nullptr)
        {
            size = 0;
            // maybe not
            //capacity = 16;
            //arr = std::make_unique<T[]>(capacity);  
        }
    }

    DynArray(const DynArray& other) : size(other.size), capacity(other.capacity)
    {
        // make deep copy
        arr = std::make_unique<T[]>(other.capacity);
        // nitpick: replace with:
        // std::copy(std::begin(other.arr), std::end(other.arr), std::begin(arr));
        for (uint i = 0; i < other.size; i++)
        {
            arr[i] = other.arr[i];
        }
    }

    DynArray(DynArray&& other) noexcept : size(other.size), capacity(other.capacity), arr(std::move(other.arr))
    {
        //other.arr = nullptr;
        //other.capacity = 0;
        //other.size = 0;
    }

    // i think you can leave this out, compiler will use the copy ctor, maybe
    /*
    DynArray& operator=(const DynArray& rhs)
    {
        assert(arr != nullptr);
        size = rhs.size;
        capacity = rhs.capacity;
        // make deep copy
        arr = std::make_unique<T[]>(rhs.capacity);
        for (uint i = 0; i < rhs.size; i++)
        {
            arr[i] = rhs.arr[i];
        }
        return *this;
    }

    // same, but with move ctor
    DynArray& operator=(const DynArray&& rhs)
    {
        assert(0); // not tested
        size = rhs.size;
        capacity = rhs.capacity;
        arr = std::move(rhs.arr);
        rhs.arr = nullptr;
        rhs.capacity = 0;
        rhs.size = 0;
        return *this;
    }
    */

    std::unique_ptr<T[]> arr;
    uint capacity;
    uint size;
};
