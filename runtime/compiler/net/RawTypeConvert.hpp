/*******************************************************************************
 * Copyright IBM Corp. and others 2020
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#ifndef RAW_TYPE_CONVERT_H
#define RAW_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include <type_traits>
#include "StreamExceptions.hpp"
#include "omrcomp.h"
#include "net/Message.hpp"

class J9Class;
class TR_ResolvedJ9Method;

namespace JITServer {
// Sending void over the network is awkward, so we just
// wrap a bool.
struct Void {
    Void() {}

    Void(bool) {}

    operator bool() const { return false; }
};

// The following are the template functions used to serialize/deserialize data.
// RawTypeConvert<T>::onRecv(Message::DataDescriptor *desc) - given a data descriptor,
// deserializes it into a value of type T and returns it.
//
// RawTypeConvert::onSend(Message &msg, const T&val) - given a value of type T,
// serializes it into an instance of Message::DataDescriptor and adds it to the message.
// Returns the number of data bytes written.
template<typename T, typename = void> struct RawTypeConvert {};

template<> struct RawTypeConvert<uint32_t> {
    static inline uint32_t onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::UINT32, "onRecv type missmatch UINT32");
        TR_ASSERT(desc->getPayloadSize() == sizeof(uint32_t), "onRecv size missmatch");
        return *static_cast<uint32_t *>(desc->getDataStart());
    }

    static inline uint32_t onSend(Message &msg, const uint32_t &val)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::UINT32, sizeof(uint32_t)), &val);
    }
};

template<> struct RawTypeConvert<uint64_t> {
    static inline uint64_t onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::UINT64, "onRecv type missmatch UINT64");
        TR_ASSERT(desc->getPayloadSize() == sizeof(uint64_t), "onRecv size missmatch");
        return *static_cast<uint64_t *>(desc->getDataStart());
    }

    static inline uint32_t onSend(Message &msg, const uint64_t &val)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::UINT64, sizeof(uint64_t)), &val,
            true);
    }
};

template<> struct RawTypeConvert<int32_t> {
    static inline int32_t onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::INT32, "onRecv type missmatch INT32");
        TR_ASSERT(desc->getPayloadSize() == sizeof(int32_t), "onRecv size missmatch");
        return *static_cast<int32_t *>(desc->getDataStart());
    }

    static inline uint32_t onSend(Message &msg, const int32_t &val)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::INT32, sizeof(int32_t)), &val);
    }
};

template<> struct RawTypeConvert<int64_t> {
    static inline int64_t onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::INT64, "onRecv type missmatch INT64");
        TR_ASSERT(desc->getPayloadSize() == sizeof(int64_t), "onRecv size missmatch");
        return *static_cast<int64_t *>(desc->getDataStart());
    }

    static inline uint32_t onSend(Message &msg, const int64_t &val)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::INT64, sizeof(int64_t)), &val,
            true);
    }
};

template<> struct RawTypeConvert<bool> {
    static inline bool onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::BOOL, "onRecv type missmatch BOOL");
        TR_ASSERT(desc->getTotalSize() == 4 && desc->getPaddingSize() == 3, "onRecv size missmatch");
        return *static_cast<bool *>(desc->getDataStart());
    }

    static inline uint32_t onSend(Message &msg, const bool &val)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::BOOL, sizeof(bool)), &val);
    }
};

template<> struct RawTypeConvert<const std::string> {
    static inline std::string onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::STRING, "onRecv type missmatch STRING");
        return std::string(static_cast<char *>(desc->getDataStart()), desc->getPayloadSize());
    }

    static inline uint32_t onSend(Message &msg, const std::string &value)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::STRING, value.length()),
            &value[0]);
    }
};

template<> struct RawTypeConvert<std::string> : RawTypeConvert<const std::string> {};

// For trivially copyable classes
template<typename T> struct RawTypeConvert<T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type> {
    static inline T onRecv(Message::DataDescriptor *desc)
    {
        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::OBJECT, "onRecv type missmatch OBJECT");
        // Validate that the payload is large enough to hold a T.
        // getPayloadSize() is derived from untrusted wire fields; an attacker could craft
        // a descriptor whose payload is smaller than sizeof(T), causing an OOB read.
        if (desc->getPayloadSize() < sizeof(T)) {
            throw JITServer::StreamFailure(std::string("OBJECT payload too small for the expected type"));
        }
        return *static_cast<T *>(desc->getDataStart());
    }

    static inline uint32_t onSend(Message &msg, const T &value)
    {
        return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::OBJECT, sizeof(T)), &value,
            sizeof(T) > 4);
    }
};

// For vectors
template<typename T>
struct RawTypeConvert<T, typename std::enable_if<std::is_same<T, std::vector<typename T::value_type> >::value>::type> {
    static inline T onRecv(Message::DataDescriptor *desc)
    {
        if (desc->getDataType() == Message::DataDescriptor::DataType::EMPTY_VECTOR) {
            return std::vector<typename T::value_type>();
        }
        if (desc->getDataType() == Message::DataDescriptor::DataType::SIMPLE_VECTOR) {
            // The element size is read from the wire and must match the expected type.
            if (desc->getVectorElementSize() != sizeof(typename T::value_type)) {
                throw JITServer::StreamFailure("SIMPLE_VECTOR element size mismatch");
            }
            // Guard against a zero element size to prevent division by zero and ensure
            // getPayloadSize() / elementSize is well defined.
            if (desc->getVectorElementSize() == 0) {
                throw JITServer::StreamFailure("SIMPLE_VECTOR element size is zero");
            }
            typename T::value_type *dataStart = static_cast<typename T::value_type *>(desc->getDataStart());
            typename T::value_type *dataEnd = reinterpret_cast<typename T::value_type *>(
                reinterpret_cast<uintptr_t>(dataStart) + desc->getPayloadSize());
            return T(dataStart, dataEnd);
        }

        TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::VECTOR, "onRecv type missmatch VECTOR");

        // Compute the exclusive end boundary of the outer vector descriptor's data payload.
        // No element descriptor or its data may begin at or beyond this address.
        const char *vectorDataEnd = static_cast<const char *>(desc->getDataStart()) + desc->getTotalSize();

        // The first element is another descriptor to represent the number of elements in the vector.
        // Before dereferencing sizeDesc, verify that both the descriptor struct itself and its
        // attached uint32_t payload fit within vectorDataEnd.
        // Minimum payload: sizeof(DataDescriptor) for sizeDesc + sizeof(uint32_t) rounded to 4 bytes.
        static constexpr uint32_t minVectorPayload = sizeof(Message::DataDescriptor) + sizeof(uint32_t);
        if (desc->getTotalSize() < minVectorPayload) {
            throw JITServer::StreamFailure("VECTOR payload too small to contain size descriptor");
        }
        Message::DataDescriptor *sizeDesc = static_cast<Message::DataDescriptor *>(desc->getDataStart());
        // Verify that sizeDesc's own descriptor struct fits within the vector's bounds.
        if (reinterpret_cast<const char *>(sizeDesc) + sizeof(Message::DataDescriptor) > vectorDataEnd) {
            throw JITServer::StreamFailure("size descriptor is outside vector bounds");
        }
        sizeDesc->validate();
        // Verify that sizeDesc's data (the element count) also fits within the vector's bounds.
        if (reinterpret_cast<const char *>(sizeDesc->getDataStart()) + sizeDesc->getTotalSize() > vectorDataEnd) {
            throw JITServer::StreamFailure(
                "JITServer detected a malformed message: size descriptor data is outside vector bounds");
        }
        uint32_t size = RawTypeConvert<uint32_t>::onRecv(sizeDesc);

        // Each iteration of the loop below reads at least one DataDescriptor (8 bytes) plus its
        // data. If the claimed element count would require more space than the outer descriptor's
        // payload actually contains, the message is malformed.
        if (size > desc->getTotalSize() / sizeof(Message::DataDescriptor)) {
            throw JITServer::StreamFailure("vector element count exceeds available data");
        }

        std::vector<typename T::value_type> values;
        values.reserve(size);

        // Find the descriptor of the first element in the array
        auto curDesc = sizeDesc->getNextDescriptor();
        for (uint32_t i = 0; i < size; ++i) {
            // Verify that the current element descriptor lies fully within the outer vector's data.
            if (reinterpret_cast<const char *>(curDesc) + sizeof(Message::DataDescriptor) > vectorDataEnd) {
                throw JITServer::StreamFailure("vector element descriptor is outside vector bounds");
            }
            curDesc->validate();
            // Verify that the element's data also lies within the outer vector's data.
            if (reinterpret_cast<const char *>(curDesc->getDataStart()) + curDesc->getTotalSize() > vectorDataEnd) {
                throw JITServer::StreamFailure("vector element data is outside vector bounds");
            }
            values.push_back(RawTypeConvert<typename T::value_type>::onRecv(curDesc));
            curDesc = curDesc->getNextDescriptor();
        }
        return values;
    }

    static inline uint32_t onSend(Message &msg, const T &value)
    {
        if (value.size() == 0) {
            // Empty vector. Write just a descriptor followed by no data
            return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::EMPTY_VECTOR, 0), 0);
        }
        // If the vector is for trivially copyable elements and
        // the size of such an element is smaller than 256,
        // copy the data as a contiguous region of memory
        if (std::is_trivially_copyable<typename T::value_type>::value) {
            size_t elemSize = sizeof(typename T::value_type);
            if (elemSize <= 0xff) {
                uint32_t payloadSize = elemSize * value.size();
                Message::DataDescriptor desc(Message::DataDescriptor::DataType::SIMPLE_VECTOR, payloadSize);
                // Need to store the size of an element in the descriptor
                desc.setVectorElementSize(elemSize);
                return msg.addData(desc, value.data(), elemSize > 4);
            }
        }
        // Complex vectors here
        uint32_t descIndex = msg.getNumDescriptors();
        uint32_t descOffset = msg.reserveDescriptor();

        // Write the size of the vector as a data point
        uint32_t totalSize = (value.size() + 1) * sizeof(Message::DataDescriptor);
        totalSize += RawTypeConvert<uint32_t>::onSend(msg, value.size());
        for (int32_t i = 0; i < value.size(); ++i) {
            // Serialize each element in a vector as its own datapoint
            totalSize += RawTypeConvert<typename T::value_type>::onSend(msg, value[i]);
        }

        // Only get pointer to the vector descriptor after all of its elements were serialized,
        // because buffer might have been reallocated
        Message::DataDescriptor *desc = msg.getDescriptor(descIndex);
        // If each element is 32-bit aligned, then the entire vector is 32-bit aligned and padding is 0
        desc->init(Message::DataDescriptor::DataType::VECTOR, totalSize, 0, 0);
        return totalSize;
    }
};

// For tuples
//
// used to unpack a tuple into variadic args
template<size_t... I> struct index_tuple_raw {
    template<size_t N> using type = index_tuple_raw<I..., N>;
};

template<size_t N> struct index_tuple_gen_raw {
    using type = typename index_tuple_gen_raw<N - 1>::type::template type<N - 1>;
};

template<> struct index_tuple_gen_raw<0> {
    using type = index_tuple_raw<>;
};

template<size_t n, typename Arg1, typename... Args> struct TupleTypeConvert {
    static inline std::tuple<Arg1, Args...> onRecvImpl(Message::DataDescriptor *desc, const char *tupleDataEnd)
    {
        // Validate current element descriptor and its data before reading it.
        if (reinterpret_cast<const char *>(desc->getDataStart()) + desc->getTotalSize() > tupleDataEnd) {
            throw JITServer::StreamFailure("tuple element data is outside tuple bounds");
        }
        // Compute the next element descriptor and verify it fits before recursing.
        Message::DataDescriptor *nextDesc = desc->getNextDescriptor();
        if (reinterpret_cast<const char *>(nextDesc) + sizeof(Message::DataDescriptor) > tupleDataEnd) {
            throw JITServer::StreamFailure("next tuple element descriptor is outside tuple bounds");
        }
        nextDesc->validate();
        return std::tuple_cat(TupleTypeConvert<n, Arg1>::onRecvImpl(desc, tupleDataEnd),
            TupleTypeConvert<n + 1, Args...>::onRecvImpl(nextDesc, tupleDataEnd));
    }

    static inline uint32_t onSendImpl(Message &msg, const Arg1 &arg1, const Args &...args)
    {
        uint32_t totalSize = TupleTypeConvert<n, Arg1>::onSendImpl(msg, arg1);
        totalSize += TupleTypeConvert<n + 1, Args...>::onSendImpl(msg, args...);
        return totalSize;
    }
};

template<size_t n, typename Arg1> struct TupleTypeConvert<n, Arg1> {
    static inline std::tuple<Arg1> onRecvImpl(Message::DataDescriptor *desc, const char *tupleDataEnd)
    {
        // Validate the last element descriptor's data before reading it.
        if (reinterpret_cast<const char *>(desc->getDataStart()) + desc->getTotalSize() > tupleDataEnd) {
            throw JITServer::StreamFailure("last tuple element data is outside tuple bounds");
        }
        return std::make_tuple(RawTypeConvert<Arg1>::onRecv(desc));
    }

    static inline uint32_t onSendImpl(Message &msg, const Arg1 &arg1)
    {
        return RawTypeConvert<Arg1>::onSend(msg, arg1) + sizeof(Message::DataDescriptor);
    }
};

template<typename... T> struct RawTypeConvert<const std::tuple<T...> > {
    static inline std::tuple<T...> onRecv(Message::DataDescriptor *desc)
    {
        if (desc->getDataType() != Message::DataDescriptor::DataType::TUPLE) {
            throw JITServer::StreamFailure("onRecv type missmatch TUPLE");
        }
        // Compute the exclusive end boundary of this tuple's payload.
        // The tuple has sizeof...(T) elements; each occupies at least one DataDescriptor.
        // Reject trivially impossible payloads before walking any element descriptor.
        static constexpr uint32_t minTuplePayload = sizeof...(T) * sizeof(Message::DataDescriptor);
        if (desc->getTotalSize() < minTuplePayload) {
            throw JITServer::StreamFailure("TUPLE payload too small for the expected element count");
        }
        const char *tupleDataEnd = static_cast<const char *>(desc->getDataStart()) + desc->getTotalSize();
        // Verify that the first element descriptor fits before handing control to onRecvImpl.
        auto *firstElemDesc = static_cast<Message::DataDescriptor *>(desc->getDataStart());
        if (reinterpret_cast<const char *>(firstElemDesc) + sizeof(Message::DataDescriptor) > tupleDataEnd) {
            throw JITServer::StreamFailure("first tuple element descriptor is outside tuple bounds");
        }
        return TupleTypeConvert<0, T...>::onRecvImpl(firstElemDesc, tupleDataEnd);
    }

    template<typename Tuple, size_t... Idx>
    static inline uint32_t onSendImpl(Message &msg, const Tuple &val, index_tuple_raw<Idx...>)
    {
        uint32_t descIndex = msg.getNumDescriptors();
        uint32_t descOffset = msg.reserveDescriptor();
        uint32_t totalSize = TupleTypeConvert<0, T...>::onSendImpl(msg, std::get<Idx>(val)...);
        // Only get pointer to the tuple descriptor after all of its elements were serialized,
        // because buffer might have been reallocated
        Message::DataDescriptor *desc = msg.getDescriptor(descIndex);
        // If each element is 32-bit aligned, then the entire tuple is 32-bit aligned and padding is 0
        desc->init(Message::DataDescriptor::DataType::TUPLE, totalSize, 0, 0);
        return totalSize;
    }

    static inline uint32_t onSend(Message &msg, const std::tuple<T...> &val)
    {
        using Idx = typename index_tuple_gen_raw<sizeof...(T)>::type;
        return onSendImpl(msg, val, Idx());
    }
};

template<typename... T> struct RawTypeConvert<std::tuple<T...> > : RawTypeConvert<const std::tuple<T...> > {};

// setArgs fills out a message with values from a variadic argument list.
// calls RawTypeConvert::onSend for each argument.
template<typename Arg1, typename... Args> struct SetArgsRaw {
    static void setArgs(Message &message, Arg1 &arg1, Args &...args)
    {
        SetArgsRaw<Arg1>::setArgs(message, arg1);
        SetArgsRaw<Args...>::setArgs(message, args...);
    }
};

template<typename Arg1> struct SetArgsRaw<Arg1> {
    static void setArgs(Message &message, Arg1 &arg1) { RawTypeConvert<Arg1>::onSend(message, arg1); }
};

template<typename... Args> void setArgsRaw(Message &message, Args &...args)
{
    message.getMetaData()->_numDataPoints = sizeof...(args);
    SetArgsRaw<Args...>::setArgs(message, args...);
}

template<typename Arg1, typename... Args> struct GetArgsRaw {
    static std::tuple<Arg1, Args...> getArgs(const Message &message, size_t n)
    {
        return std::tuple_cat(GetArgsRaw<Arg1>::getArgs(message, n), GetArgsRaw<Args...>::getArgs(message, n + 1));
    }
};

template<typename Arg> struct GetArgsRaw<Arg> {
    static std::tuple<Arg> getArgs(const Message &message, size_t n)
    {
        Message::DataDescriptor *desc = message.getDescriptor(n);
        return std::make_tuple(RawTypeConvert<Arg>::onRecv(desc));
    }
};

template<typename... Args> std::tuple<Args...> getArgsRaw(const Message &message)
{
    if (sizeof...(Args) != message.getMetaData()->_numDataPoints)
        throw StreamArityMismatch("Received " + std::to_string(message.getMetaData()->_numDataPoints)
            + " args to unpack but expect " + std::to_string(sizeof...(Args)) + "-tuple");
    return GetArgsRaw<Args...>::getArgs(message, 0);
}
}; // namespace JITServer
#endif
