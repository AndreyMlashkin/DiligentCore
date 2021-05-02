/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

#include "BasicTypes.h"

namespace Diligent
{

template <typename IndexType>
struct TIndexWrapper
{
public:
    TIndexWrapper() {}

    explicit TIndexWrapper(Uint32 Value) :
        m_Value{static_cast<IndexType>(Value)}
    {
        VERIFY(m_Value == Value, "Not enought bits to store value");
    }

    TIndexWrapper(const TIndexWrapper&) = default;
    TIndexWrapper(TIndexWrapper&&)      = default;

    operator Uint32() const
    {
        return m_Value;
    }

    bool operator==(const TIndexWrapper& Other) const
    {
        return m_Value == Other.m_Value;
    }

    struct Hasher
    {
        size_t operator()(const TIndexWrapper& Idx) const
        {
            return size_t{Idx.m_Value};
        }
    };

private:
    IndexType m_Value = 0;
};

using HardwareQueueId   = TIndexWrapper<Uint8>;
using CommandQueueIndex = TIndexWrapper<Uint8>;
using ContextIndex      = TIndexWrapper<Uint8>;

} // namespace Diligent