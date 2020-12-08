/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "DynamicAtlasManager.hpp"

#include <climits>

#include "AdvancedMath.hpp"

namespace Diligent
{

static const DynamicAtlasManager::Region InvalidRegion{UINT_MAX, UINT_MAX, 0, 0};
static const DynamicAtlasManager::Region AllocatedRegion{UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};

#if DILIGENT_DEBUG
void DynamicAtlasManager::Node::Validate() const
{
    VERIFY(NumChildren == 0 || NumChildren >= 2, "Zero or at least two children are expected");
    VERIFY(NumChildren == 0 || !IsAllocated, "Allocated nodes must not have children");
    if (NumChildren > 0)
    {
        Uint32 Area = 0;
        for (Uint32 i = 0; i < NumChildren; ++i)
        {
            const auto& R0 = Child(i).R;

            VERIFY(!R0.IsEmpty(), "Region must not be empty");
            VERIFY(R0.x >= R.x && R0.x + R0.width <= R.x + R.width && R0.y >= R.y && R0.y + R0.height <= R.y + R.height,
                   "Region lies outside of parent region");

            Area += R0.width * R0.height;

            for (Uint32 j = i + 1; j < NumChildren; ++j)
            {
                const auto& R1 = Child(j).R;
                if (CheckBox2DBox2DOverlap<false>(uint2{R0.x, R0.y}, uint2{R0.x + R0.width, R0.y + R0.height},
                                                  uint2{R1.x, R1.y}, uint2{R1.x + R1.width, R1.y + R1.height}))
                {
                    UNEXPECTED("Regions [", R0.x, ", ", R0.x + R0.width, ") x [", R0.y, ", ", R0.y + R0.height,
                               ") and [", R1.x, ", ", R1.x + R1.width, ") x [", R1.y, ", ", R1.y + R1.height, ") overlap");
                }
            }
        }
        VERIFY(Area == R.width * R.height, "Children do not cover entire parent region");
    }
}
#endif

void DynamicAtlasManager::Node::Split(const std::initializer_list<Region>& Regions)
{
    VERIFY(Regions.size() >= 2, "There must be at least two regions");
    VERIFY(!HasChildren(), "This node already has children and can't be split");
    VERIFY(!IsAllocated, "Allocated region can't be split");

    Children.reset(new Node[Regions.size()]);
    NumChildren = 0;
    for (const auto& _R : Regions)
    {
        Children[NumChildren].Parent = this;
        Children[NumChildren].R      = _R;
        ++NumChildren;
    }
    VERIFY_EXPR(NumChildren == Regions.size());

#if DILIGENT_DEBUG
    Validate();
#endif
}

bool DynamicAtlasManager::Node::CanMergeChildren() const
{
    bool CanMerge = true;
    for (Uint32 i = 0; i < NumChildren && CanMerge; ++i)
        CanMerge = !Child(i).IsAllocated && !Child(i).HasChildren();

    return CanMerge;
}

void DynamicAtlasManager::Node::MergeChildren()
{
    VERIFY_EXPR(HasChildren());
    VERIFY_EXPR(CanMergeChildren());
    Children.reset();
    NumChildren = 0;
}


DynamicAtlasManager::DynamicAtlasManager(Uint32 Width, Uint32 Height) :
    m_Width{Width},
    m_Height{Height}
{
    m_Root->R = Region{0, 0, Width, Height};
    RegisterNode(*m_Root);
}


DynamicAtlasManager::~DynamicAtlasManager()
{
    if (m_Root)
    {
#if DILIGENT_DEBUG
        DbgVerifyConsistency();
#endif

        DEV_CHECK_ERR(!m_Root->IsAllocated && !m_Root->HasChildren(), "Root node is expected to be free");
        VERIFY_EXPR(m_FreeRegionsByWidth.size() == m_FreeRegionsByHeight.size());
        DEV_CHECK_ERR(m_FreeRegionsByWidth.size() == 1, "There expected to be a single free region");
        DEV_CHECK_ERR(m_AllocatedRegions.empty(), "There must be no allocated regions");
    }
    else
    {
        VERIFY_EXPR(m_FreeRegionsByWidth.empty());
        VERIFY_EXPR(m_FreeRegionsByHeight.empty());
        VERIFY_EXPR(m_AllocatedRegions.empty());
    }
}

void DynamicAtlasManager::RegisterNode(Node& N)
{
    VERIFY(!N.HasChildren(), "Registering node that has children");
    VERIFY(!N.R.IsEmpty(), "Region must not be empty");

    if (N.IsAllocated)
    {
        VERIFY(m_AllocatedRegions.find(N.R) == m_AllocatedRegions.end(), "New region should not be present in allocated regions hash map");
        m_AllocatedRegions.emplace(N.R, &N);
    }
    else
    {
        VERIFY(m_FreeRegionsByWidth.find(N.R) == m_FreeRegionsByWidth.end(), "New region should not be present in free regions map");
        VERIFY(m_FreeRegionsByHeight.find(N.R) == m_FreeRegionsByHeight.end(), "New region should not be present in free regions map");
        m_FreeRegionsByWidth.emplace(N.R, &N);
        m_FreeRegionsByHeight.emplace(N.R, &N);
    }
}

void DynamicAtlasManager::UnregisterNode(const Node& N)
{
    VERIFY(!N.HasChildren(), "Unregistering node that has children");
    VERIFY(!N.R.IsEmpty(), "Region must not be empty");

    if (N.IsAllocated)
    {
        VERIFY(m_AllocatedRegions.find(N.R) != m_AllocatedRegions.end(), "Region is not found in allocated regions hash map");
        m_AllocatedRegions.erase(N.R);
    }
    else
    {
        VERIFY(m_FreeRegionsByWidth.find(N.R) != m_FreeRegionsByWidth.end(), "Region is not found in free regions map");
        VERIFY(m_FreeRegionsByHeight.find(N.R) != m_FreeRegionsByHeight.end(), "Region is not is not in free regions map");
        m_FreeRegionsByWidth.erase(N.R);
        m_FreeRegionsByHeight.erase(N.R);
    }
}



DynamicAtlasManager::Region DynamicAtlasManager::Allocate(Uint32 Width, Uint32 Height)
{
    auto it_w = m_FreeRegionsByWidth.lower_bound(Region{0, 0, Width, 0});
    while (it_w != m_FreeRegionsByWidth.end() && it_w->first.height < Height)
        ++it_w;
    VERIFY_EXPR(it_w == m_FreeRegionsByWidth.end() || (it_w->first.width >= Width && it_w->first.height >= Height));

    auto it_h = m_FreeRegionsByHeight.lower_bound(Region{0, 0, 0, Height});
    while (it_h != m_FreeRegionsByHeight.end() && it_h->first.width < Width)
        ++it_h;
    VERIFY_EXPR(it_h == m_FreeRegionsByHeight.end() || (it_h->first.width >= Width && it_h->first.height >= Height));

    const auto AreaW = it_w != m_FreeRegionsByWidth.end() ? it_w->first.width * it_w->first.height : 0;
    const auto AreaH = it_h != m_FreeRegionsByHeight.end() ? it_h->first.width * it_h->first.height : 0;
    VERIFY_EXPR(AreaW == 0 || AreaW >= Width * Height);
    VERIFY_EXPR(AreaH == 0 || AreaH >= Width * Height);

    Node* pSrcNode = nullptr;
    // Use the smaller area source region
    if (AreaW > 0 && AreaH > 0)
    {
        pSrcNode = AreaW < AreaH ? it_w->second : it_h->second;
    }
    else if (AreaW > 0)
    {
        pSrcNode = it_w->second;
    }
    else if (AreaH > 0)
    {
        pSrcNode = it_h->second;
    }
    else
    {
        return Region{};
    }

    UnregisterNode(*pSrcNode);

    const auto& R = pSrcNode->R;
    if (R.width > Width && R.height > Height)
    {
        if (R.width > R.height)
        {
            //    _____________________
            //   |       |             |
            //   |   B   |             |
            //   |_______|      A      |
            //   |       |             |
            //   |   R   |             |
            //   |_______|_____________|
            //
            pSrcNode->Split(
                {
                    // clang-format off
                    Region{R.x,         R.y,          Width,           Height           }, // R
                    Region{R.x + Width, R.y,          R.width - Width, R.height         }, // A
                    Region{R.x,         R.y + Height, Width,           R.height - Height}  // B
                    // clang-format on
                });
        }
        else
        {
            //   _____________
            //  |             |
            //  |             |
            //  |      A      |
            //  |             |
            //  |_____ _______|
            //  |     |       |
            //  |  R  |   B   |
            //  |_____|_______|
            //
            pSrcNode->Split(
                {
                    // clang-format off
                    Region{R.x,         R.y,          Width,           Height           }, // R
                    Region{R.x,         R.y + Height, R.width,         R.height - Height}, // A
                    Region{R.x + Width, R.y,          R.width - Width, Height           }  // B
                    // clang-format on
                });
        }
    }
    else if (R.width > Width)
    {
        //   _______ __________
        //  |       |          |
        //  |   R   |    A     |
        //  |_______|__________|
        //
        pSrcNode->Split(
            {
                // clang-format off
                Region{R.x,         R.y, Width,           Height  }, // R
                Region{R.x + Width, R.y, R.width - Width, R.height}  // A
                // clang-format on
            });
    }
    else if (R.height > Height)
    {
        //    _______
        //   |       |
        //   |   A   |
        //   |_______|
        //   |       |
        //   |   R   |
        //   |_______|
        //
        pSrcNode->Split(
            {
                // clang-format off
                Region{R.x,          R.y,   Width, Height           }, // R
                Region{R.x, R.y + Height, R.width, R.height - Height}  // A
                // clang-format on
            });
    }

    if (pSrcNode->HasChildren())
    {
        pSrcNode->Child(0).IsAllocated = true;
        pSrcNode->ProcessChildren([this](Node& Child) //
                                  {
                                      RegisterNode(Child);
                                  });
    }
    else
    {
        pSrcNode->IsAllocated = true;
        RegisterNode(*pSrcNode);
    }

#if DILIGENT_DEBUG
    DbgVerifyConsistency();
#endif

    return pSrcNode->HasChildren() ? pSrcNode->Child(0).R : pSrcNode->R;
}


void DynamicAtlasManager::Free(Region&& R)
{
#if DILIGENT_DEBUG
    DbgVerifyRegion(R);
#endif

    auto node_it = m_AllocatedRegions.find(R);
    if (node_it == m_AllocatedRegions.end())
    {
        UNEXPECTED("Unable to find region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height, ") among allocated regions.");
        return;
    }

    VERIFY_EXPR(node_it->first == R && node_it->second->R == R);
    auto* N = node_it->second;
    VERIFY_EXPR(N->IsAllocated && !N->HasChildren());
    UnregisterNode(*N);
    N->IsAllocated = false;
    RegisterNode(*N);

    N = N->Parent;
    while (N != nullptr && N->CanMergeChildren())
    {
        N->ProcessChildren([this](const Node& Child) //
                           {
                               UnregisterNode(Child);
                           });
        N->MergeChildren();
        RegisterNode(*N);

        N = N->Parent;
    }

#if DILIGENT_DEBUG
    DbgVerifyConsistency();
#endif

    R = InvalidRegion;
}

#if 0
void DynamicAtlasManager::AddFreeRegion(Region R)
{
#    ifdef DILIGENT_DEBUG
    {
        const auto& R0 = GetRegion(R.x, R.y);
        VERIFY_EXPR(R0 == AllocatedRegion || R0 == InvalidRegion);
        for (Uint32 y = R.y; y < R.y + R.height; ++y)
        {
            for (Uint32 x = R.x; x < R.x + R.width; ++x)
            {
                VERIFY_EXPR(GetRegion(x, y) == R0);
            }
        }
    }
#    endif

    bool Merged = false;
    do
    {
        auto TryMergeHorz = [&]() //
        {
            if (R.x > 0)
            {
                const auto& lftR = GetRegion(R.x - 1, R.y);
                if (lftR != AllocatedRegion && lftR != InvalidRegion)
                {
                    VERIFY_EXPR(lftR.x + lftR.width == R.x);
                    if (lftR.y == R.y && lftR.height == R.height)
                    {
                        //   __________ __________
                        //  |          |          |
                        //  |   lftR   |    R     |
                        //  |__________|__________|
                        R.x = lftR.x;
                        R.width += lftR.width;
                        RemoveFreeRegion(lftR);
                        VERIFY_EXPR(lftR == InvalidRegion);
                        return true;
                    }
                }
            }

            if (R.x + R.width < m_Width)
            {
                const auto& rgtR = GetRegion(R.x + R.width, R.y);
                if (rgtR != AllocatedRegion && rgtR != InvalidRegion)
                {
                    VERIFY_EXPR(R.x + R.width == rgtR.x);
                    if (rgtR.y == R.y && rgtR.height == R.height)
                    {
                        //   _________ ____________
                        //  |         |            |
                        //  |    R    |    rgtR    |
                        //  |_________|____________|
                        R.width += rgtR.width;
                        RemoveFreeRegion(rgtR);
                        VERIFY_EXPR(rgtR == InvalidRegion);
                        return true;
                    }
                }
            }

            return false;
        };

        auto TryMergeVert = [&]() //
        {
            if (R.y > 0)
            {
                const auto& btmR = GetRegion(R.x, R.y - 1);
                if (btmR != AllocatedRegion && btmR != InvalidRegion)
                {
                    VERIFY_EXPR(btmR.y + btmR.height == R.y);
                    if (btmR.x == R.x && btmR.width == R.width)
                    {
                        //    ________
                        //   |        |
                        //   |   R    |
                        //   |________|
                        //   |        |
                        //   |  btmR  |
                        //   |________|
                        R.y = btmR.y;
                        R.height += btmR.height;
                        RemoveFreeRegion(btmR);
                        VERIFY_EXPR(btmR == InvalidRegion);
                        return true;
                    }
                }
            }

            if (R.y + R.height < m_Height)
            {
                const auto& tpR = GetRegion(R.x, R.y + R.height);
                if (tpR != AllocatedRegion && tpR != InvalidRegion)
                {
                    VERIFY_EXPR(R.y + R.height == tpR.y);
                    if (tpR.x == R.x && tpR.width == R.width)
                    {
                        //    _______
                        //   |       |
                        //   |  tpR  |
                        //   |_______|
                        //   |       |
                        //   |   R   |
                        //   |_______|
                        R.height += tpR.height;
                        RemoveFreeRegion(tpR);
                        VERIFY_EXPR(tpR == InvalidRegion);
                        return true;
                    }
                }
            }

            return false;
        };

        // Try to merge along the longest edge first
        Merged = (R.width > R.height) ? TryMergeVert() : TryMergeHorz();

        // If not merged, try another edge
        if (!Merged)
        {
            Merged = (R.width > R.height) ? TryMergeHorz() : TryMergeVert();
        }
    } while (Merged);

    InitRegion(R, R);

    {
        auto inserted = m_FreeRegionsByWidth.emplace(R).second;
        VERIFY_EXPR(inserted);
    }
    {
        auto inserted = m_FreeRegionsByHeight.emplace(R).second;
        VERIFY_EXPR(inserted);
    }

#    if DILIGENT_DEBUG
    {
        auto inserted = m_dbgRegions.emplace(R, false).second;
        VERIFY_EXPR(inserted);
    }
#    endif
}

#endif

#if DILIGENT_DEBUG

void DynamicAtlasManager::DbgVerifyRegion(const Region& R) const
{
    VERIFY_EXPR(R != InvalidRegion && R != AllocatedRegion);
    VERIFY_EXPR(!R.IsEmpty());

    VERIFY(R.x < m_Width, "Region x (", R.x, ") exceeds atlas width (", m_Width, ").");
    VERIFY(R.y < m_Height, "Region y (", R.y, ") exceeds atlas height (", m_Height, ").");
    VERIFY(R.x + R.width <= m_Width, "Region right boundary (", R.x + R.width, ") exceeds atlas width (", m_Width, ").");
    VERIFY(R.y + R.height <= m_Height, "Region top boundart (", R.y + R.height, ") exceeds atlas height (", m_Height, ").");
}

void DynamicAtlasManager::DbgRecursiveVerifyConsistency(const Node& N, Uint32& Area) const
{
    N.Validate();
    if (N.HasChildren())
    {
        VERIFY_EXPR(!N.IsAllocated);
        VERIFY(m_AllocatedRegions.find(N.R) == m_AllocatedRegions.end(), "Region with children should not be present in allocated regions hash map");
        VERIFY(m_FreeRegionsByWidth.find(N.R) == m_FreeRegionsByWidth.end(), "Region with children should not be present in free regions map");
        VERIFY(m_FreeRegionsByHeight.find(N.R) == m_FreeRegionsByHeight.end(), "Region with children should not be present in free regions map");

        N.ProcessChildren([&Area, this](const Node& Child) //
                          {
                              DbgRecursiveVerifyConsistency(Child, Area);
                          });
    }
    else
    {
        if (N.IsAllocated)
        {
            VERIFY(m_AllocatedRegions.find(N.R) != m_AllocatedRegions.end(), "Allocated region is not found in allocated regions hash map");
            VERIFY(m_FreeRegionsByWidth.find(N.R) == m_FreeRegionsByWidth.end(), "Allocated region should not be present in free regions map");
            VERIFY(m_FreeRegionsByHeight.find(N.R) == m_FreeRegionsByHeight.end(), "Allocated region should not be present in free regions map");
        }
        else
        {
            VERIFY(m_AllocatedRegions.find(N.R) == m_AllocatedRegions.end(), "Free region is found in allocated regions hash map");
            VERIFY(m_FreeRegionsByWidth.find(N.R) != m_FreeRegionsByWidth.end(), "Free region should is not found in free regions map");
            VERIFY(m_FreeRegionsByHeight.find(N.R) != m_FreeRegionsByHeight.end(), "Free region should is not found in free regions map");
        }

        Area += N.R.width * N.R.height;
    }
}

void DynamicAtlasManager::DbgVerifyConsistency() const
{
    VERIFY_EXPR(m_FreeRegionsByWidth.size() == m_FreeRegionsByHeight.size());
    Uint32 Area = 0;

    DbgRecursiveVerifyConsistency(*m_Root, Area);

    VERIFY(Area == m_Width * m_Height, "Not entire atlas area has been covered");
}
#endif


} // namespace Diligent
