// Translated to Slang and modified from original glsl shader code by nvidia:

// Copyright 2021 NVIDIA CORPORATION
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Check required macros
#ifndef NVPRO_PYRAMID_IS_FAST_PIPELINE
#error "Missing required macro NVPRO_PYRAMID_IS_FAST_PIPELINE"
#endif
#ifndef NVPRO_PYRAMID_REDUCE
  #if !defined(NVPRO_PYRAMID_REDUCE4) || !NVPRO_PYRAMID_IS_FAST_PIPELINE
  #error "Missing required macro NVPRO_PYRAMID_REDUCE"
  #endif
#endif
#ifndef NVPRO_PYRAMID_LOAD
  #if !defined(NVPRO_PYRAMID_LOAD_REDUCE4) || !NVPRO_PYRAMID_IS_FAST_PIPELINE
  #error "Missing required macro NVPRO_PYRAMID_LOAD"
  #endif
#endif
#ifndef NVPRO_PYRAMID_STORE
#error "Missing required macro NVPRO_PYRAMID_STORE"
#endif
#ifndef NVPRO_PYRAMID_TYPE
#error "Missing required macro NVPRO_PYRAMID_TYPE"
#endif
#ifndef NVPRO_PYRAMID_LEVEL_SIZE
#error "Missing required macro NVPRO_PYRAMID_LEVEL_SIZE"
#endif

// Provide defaults for optional macros.
#ifndef NVPRO_PYRAMID_PUSH_CONSTANT
[vk_push_constant] uniform uint nvproPyramidPushConstant_;
#define NVPRO_PYRAMID_PUSH_CONSTANT nvproPyramidPushConstant_
#endif

// The mip level used as source data for the current dispatch.
// Change nvpro_pyramid_dispatch.hpp nvproPyramidInputLevelShift if changed.
#define NVPRO_PYRAMID_INPUT_LEVEL_ int(uint(NVPRO_PYRAMID_PUSH_CONSTANT) >> 5u)

// Number of subsequent mip levels to fill.
#define NVPRO_PYRAMID_LEVEL_COUNT_ int(uint(NVPRO_PYRAMID_PUSH_CONSTANT) & 31u)

#ifndef NVPRO_PYRAMID_REDUCE2
#define NVPRO_PYRAMID_REDUCE2(v0, v1, out_) \
  NVPRO_PYRAMID_REDUCE(0.5, v0, 0.5, v1, 0, v1, out_)
#endif

#ifndef NVPRO_PYRAMID_REDUCE4
#define NVPRO_PYRAMID_REDUCE4(v00, v01, v10, v11, out_) \
{ \
  NVPRO_PYRAMID_TYPE v0_, v1_; \
  NVPRO_PYRAMID_REDUCE2(v00, v01, v0_); \
  NVPRO_PYRAMID_REDUCE2(v10, v11, v1_); \
  NVPRO_PYRAMID_REDUCE2(v0_, v1_, out_); \
}
#endif

#ifndef NVPRO_PYRAMID_LOAD_REDUCE4
#define NVPRO_PYRAMID_LOAD_REDUCE4(srcCoord_, srcLevel_, out_) \
{ \
  NVPRO_PYRAMID_TYPE v00_, v01_, v10_, v11_; \
  NVPRO_PYRAMID_LOAD((srcCoord_) + int2(0, 0), srcLevel_, v00_); \
  NVPRO_PYRAMID_LOAD((srcCoord_) + int2(0, 1), srcLevel_, v01_); \
  NVPRO_PYRAMID_LOAD((srcCoord_) + int2(1, 0), srcLevel_, v10_); \
  NVPRO_PYRAMID_LOAD((srcCoord_) + int2(1, 1), srcLevel_, v11_); \
  NVPRO_PYRAMID_REDUCE4(v00_, v01_, v10_, v11_, out_); \
}
#endif

#if !defined(NVPRO_PYRAMID_SHUFFLE_XOR) && NVPRO_PYRAMID_IS_FAST_PIPELINE != 0
#define NVPRO_PYRAMID_SHUFFLE_XOR(in_, invocation, mask_) WaveShuffle(in_, invocation ^ mask_)
#endif

// Handle optional specialized shared memory type.
#ifdef NVPRO_PYRAMID_SHARED_TYPE
  #if !defined(NVPRO_PYRAMID_SHARED_LOAD) || !defined(NVPRO_PYRAMID_SHARED_STORE)
    #error "Missing NVPRO_PYRAMID_SHARED_LOAD or NVPRO_PYRAMID_SHARED_STORE; needed when NVPRO_PYRAMID_SHARED_TYPE is defined."
  #endif
#else
  #if defined(NVPRO_PYRAMID_SHARED_LOAD) || defined(NVPRO_PYRAMID_SHARED_STORE)
    #error "Missing NVPRO_PYRAMID_SHARED_TYPE, needed when NVPRO_PYRAMID_SHARED_LOAD or NVPRO_PYRAMID_SHARED_STORE is defined."
  #endif
  #define NVPRO_PYRAMID_SHARED_TYPE NVPRO_PYRAMID_TYPE
  #define NVPRO_PYRAMID_SHARED_LOAD(smem_, out_) out_ = smem_
  #define NVPRO_PYRAMID_SHARED_STORE(smem_, in_) smem_ = in_
#endif

#if NVPRO_PYRAMID_IS_FAST_PIPELINE != 0

static const uint WORK_GROUP_X = 256;

groupshared NVPRO_PYRAMID_SHARED_TYPE sharedTile_[16];

void handleTile_(int2 srcTileOffset_, int inputLevel_, uint levelCount_,
                 bool sharedMemoryWrite_, uint sharedMemoryIdx_, 
                 uint localInvocationIndex : SV_GroupIndex)
{
  NVPRO_PYRAMID_TYPE sample00_, sample01_, sample10_, sample11_, out_;
  int dstLevel_ = inputLevel_ + 1;
  int2 dstSubTile_;

  // Calculate the index of this thread within the team.
  uint teamMask_   = levelCount_ >= 3 ? 15 : levelCount_ == 2 ? 3 : 0;
  uint idxInTeam_  = localInvocationIndex & teamMask_;

  // NOTE the extra sharedMemoryWrite_ requirement!!!
  if (sharedMemoryWrite_ && levelCount_ == 4)
  {
    // The location of the sub-tile assigned to this thread in level inputLevel_
    uint  xOffset_    = (idxInTeam_ & 1) << 2 | (idxInTeam_ & 4) << 1;
    uint  yOffset_    = (idxInTeam_ & 2) << 1 | (idxInTeam_ & 8);
    int2 srcSubTile_  = srcTileOffset_ + int2(xOffset_, yOffset_);
    dstSubTile_       = srcSubTile_ >> 1;

    // Thread calculates upper-left sample of 2x2 output sub-tile.
    int2 srcCoord_, dstCoord_;
    srcCoord_ = srcSubTile_;
    dstCoord_ = dstSubTile_;
    NVPRO_PYRAMID_LOAD_REDUCE4(srcCoord_, inputLevel_, sample00_);
    NVPRO_PYRAMID_STORE(dstCoord_, dstLevel_, sample00_);

    // Thread calculates lower-left sample of 2x2 output sub-tile.
    srcCoord_ = srcSubTile_ + int2(0, 2);
    dstCoord_ = dstSubTile_ + int2(0, 1);
    NVPRO_PYRAMID_LOAD_REDUCE4(srcCoord_, inputLevel_, sample01_);
    NVPRO_PYRAMID_STORE(dstCoord_, dstLevel_, sample01_);

    // Thread calculates upper-right sample of 2x2 output sub-tile.
    srcCoord_ = srcSubTile_ + int2(2, 0);
    dstCoord_ = dstSubTile_ + int2(1, 0);
    NVPRO_PYRAMID_LOAD_REDUCE4(srcCoord_, inputLevel_, sample10_);
    NVPRO_PYRAMID_STORE(dstCoord_, dstLevel_, sample10_);

    // Thread calculates lower-right sample of 2x2 output sub-tile.
    srcCoord_ = srcSubTile_ + int2(2, 2);
    dstCoord_ = dstSubTile_ + int2(1, 1);
    NVPRO_PYRAMID_LOAD_REDUCE4(srcCoord_, inputLevel_, sample11_);
    NVPRO_PYRAMID_STORE(dstCoord_, dstLevel_, sample11_);

    // Now the full assigned 2x2 subtile has been filled, move on to
    // the 1x1 sample of the next level assigned to this thread.
    dstLevel_++;
    dstSubTile_ >>= 1;
    NVPRO_PYRAMID_REDUCE4(sample00_, sample01_, sample10_, sample11_, out_);
    NVPRO_PYRAMID_STORE(dstSubTile_, dstLevel_, out_);
  }
  else  // levelCount_ != 4
  {
    // The location of the sub-tile assigned to this thread in the
    // level after level inputLevel_.
    uint  xOffset_    = (idxInTeam_ & 1) << 1 | (idxInTeam_ & 4);
    uint  yOffset_    = (idxInTeam_ & 2) | (idxInTeam_ & 8) >> 1;
    int2 srcSubTile_  = srcTileOffset_ + int2(xOffset_, yOffset_);
    dstSubTile_       = srcSubTile_ >> 1;

    // Thread calculates the sample in that sub-tile.
    NVPRO_PYRAMID_LOAD_REDUCE4(srcSubTile_, inputLevel_, out_);
    NVPRO_PYRAMID_STORE(dstSubTile_, dstLevel_, out_);
  }

  if (!sharedMemoryWrite_ && levelCount_ == 1) return;

  // The whole team computes the 2x2 tile in the next level; only 1
  // out of every 4 threads does this. Use shuffle to get the needed
  // data from the other three threads.
  dstLevel_++;
  dstSubTile_ >>= 1;
  sample00_ = out_;
  sample01_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 1);
  sample10_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 2);
  sample11_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 3);

  if (0 == (WaveGetLaneIndex() & 3))
  {
    NVPRO_PYRAMID_REDUCE4(sample00_, sample01_, sample10_, sample11_, out_);
    NVPRO_PYRAMID_STORE(dstSubTile_, dstLevel_, out_);
  }

  if (!sharedMemoryWrite_ && levelCount_ == 2) return;

  // Compute 1x1 "tile" in the last level handled by this function;
  // only 1 thread per 16 does this. Shuffle again.
  // This is also the thread that does the optional shared memory write.
  dstLevel_++;
  dstSubTile_ >>= 1;
  sample00_ = out_;
  sample01_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 4);
  sample10_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 8);
  sample11_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 12);

  if (0 == (WaveGetLaneIndex() & 15))
  {
    NVPRO_PYRAMID_REDUCE4(sample00_, sample01_, sample10_, sample11_, out_);
    NVPRO_PYRAMID_STORE(dstSubTile_, dstLevel_, out_);
    if (sharedMemoryWrite_)
    {
      NVPRO_PYRAMID_SHARED_STORE(sharedTile_[sharedMemoryIdx_], out_);
    }
  }
}


void nvproPyramidMain(uint2 globalInvocationId : SV_DispatchThreadID, uint localInvocationIndex : SV_GroupIndex,
    uint3 workgroupId : SV_GroupID)
{
  // Cut the input mip level into square tiles of edge length
  // 2 to the power of NVPRO_PYRAMID_LEVEL_COUNT_.
  int   levelCount_      = NVPRO_PYRAMID_LEVEL_COUNT_;
  int   inputLevel_      = NVPRO_PYRAMID_INPUT_LEVEL_;
  int2 srcImageSize_     = NVPRO_PYRAMID_LEVEL_SIZE(inputLevel_);
  uint  horizontalTiles_ = uint(srcImageSize_.x) >> levelCount_;
  uint  verticalTiles_   = uint(srcImageSize_.y) >> levelCount_;

  // Calculate the team size from the level count.  Each thread
  // handles 4 inupt samples, except when levelCount_ == 6, then each
  // handles 16 samples.
  uint  teamSizeLog2_ = min(8u, levelCount_ * 2u - 2u);

  // Assign tiles to each team.
  uint  tileIndex_       = globalInvocationId.x >> teamSizeLog2_;
  uint  horizontalIndex_ = tileIndex_ % horizontalTiles_;
  uint  verticalIndex_   = tileIndex_ / horizontalTiles_;
  int2 tileOffset_ = int2(horizontalIndex_, verticalIndex_) << levelCount_;

  if (levelCount_ <= 3)
  {
    if (verticalIndex_ < verticalTiles_)
    {
      // Reminder to self: can't handle 4 level case when
      // sharedMemoryWrite_ is false.
      handleTile_(tileOffset_, inputLevel_, levelCount_, false, 0, localInvocationIndex);
    }
    return;
  }

  // For 4 or more levels, team size is too big for shuffle communication.
  // Need to split the tile into sub-tiles and teams into 16 thread sub-teams.
  // Each sub-team writes one sample to shared memory.
  // Refer to sharedTile_ diagram for details.
  if (verticalIndex_ < verticalTiles_)
  {
    // Number of levels to fill for now.
    int subLevelCount_ = levelCount_ == 6 ? 4 : 3;

    // Calculate the index of the sub-team within the team.
    int subTeamMask_ = levelCount_ == 4 ? 3 : 15;
    int subTeamIdx_  = int(globalInvocationId.x >> 4) & subTeamMask_;

    // Location of sub-tile; they are 8x8 or 16x16 depending on subLevelCount_
    int2 subTeamOffset_;
    subTeamOffset_.x = (subTeamIdx_ & 1) << 3 | (subTeamIdx_ & 4) << 2;
    subTeamOffset_.y = (subTeamIdx_ & 2) << 2 | (subTeamIdx_ & 8) << 1;
    if (subLevelCount_ == 4)
    {
      subTeamOffset_ <<= 1;
    }

    // Index in shared memory that this sub-team will write to.
    uint sharedMemoryIndex_ = (globalInvocationId.x >> 4u) & 15u;

    // Handle the sub-tile and write the last level 1x1 sample to shared memory.
    handleTile_(tileOffset_ + subTeamOffset_, inputLevel_, subLevelCount_,
                true, sharedMemoryIndex_, localInvocationIndex);

    // Problem reduces to handling 1 or 2 remaining levels.
    inputLevel_ += subLevelCount_;
    levelCount_ -= subLevelCount_;
  }

  // Wait for shared memory to fill.
  GroupMemoryBarrierWithGroupSync();

  // Handle the remaining 1 or 2 levels.
  // Only 4 threads have to do this per workgroup (NOT per team)
  if (localInvocationIndex < 4)
  {
    NVPRO_PYRAMID_TYPE in00_, in01_, in10_, in11_, out_;
    if (levelCount_ == 1)
    {
      // Handle up to 4 2x2 tiles in shared memory, 1 tile per thread.
      // Tile location calculated for the output level (final level).
      tileIndex_       = workgroupId.x * 4 + localInvocationIndex;
      horizontalIndex_ = tileIndex_ % horizontalTiles_;
      verticalIndex_   = tileIndex_ / horizontalTiles_;
      tileOffset_      = int2(horizontalIndex_, verticalIndex_);
      uint smemOffset_ = localInvocationIndex * 4u;

      if (verticalIndex_ < verticalTiles_)
      {
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 0u], in00_);
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 1u], in10_);
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 2u], in01_);
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 3u], in11_);
        NVPRO_PYRAMID_REDUCE4(in00_, in01_, in10_, in11_, out_);
        NVPRO_PYRAMID_STORE(tileOffset_, (inputLevel_ + 1), out_);
      }
    }
    else  // levelCount_ == 2
    {
      // Handle the 4x4 tile in shared memory, 1 2x2 sub-tile per
      // thread.  Tile location calculated for the final output level
      // (here we first calculate an intermediate level).
      tileIndex_       = workgroupId.x;
      horizontalIndex_ = tileIndex_ % horizontalTiles_;
      verticalIndex_   = tileIndex_ / horizontalTiles_;
      tileOffset_      = int2(horizontalIndex_, verticalIndex_);
      uint smemOffset_ = localInvocationIndex * 4u;

      if (verticalIndex_ < verticalTiles_)
      {
        // Handle first level after inputLevel_
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 0u], in00_);
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 1u], in10_);
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 2u], in01_);
        NVPRO_PYRAMID_SHARED_LOAD(sharedTile_[smemOffset_ + 3u], in11_);
        NVPRO_PYRAMID_REDUCE4(in00_, in01_, in10_, in11_, out_);
        int2 threadOffset_ = int2(localInvocationIndex & 1,
                                 (localInvocationIndex & 2) >> 1);
        NVPRO_PYRAMID_STORE((tileOffset_ * 2 + threadOffset_),
                            (inputLevel_ + 1), out_);
        // Shuffle 4 samples and produce sole last level sample.
        in00_ = out_;
        in10_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 1);
        in01_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 2);
        in11_ = NVPRO_PYRAMID_SHUFFLE_XOR(out_, WaveGetLaneIndex(), 3);

        if (localInvocationIndex == 0u)
        {
          NVPRO_PYRAMID_REDUCE4(in00_, in01_, in10_, in11_, out_);
          NVPRO_PYRAMID_STORE(tileOffset_, (inputLevel_ + 2), out_);
        }
      }
    }
  }
}

#else /* non-fast path */

static const uint WORK_GROUP_X = 4 * 32;

groupshared NVPRO_PYRAMID_SHARED_TYPE sharedLevel_[17][17]; // [y][x]

int2 kernelSizeFromInputSize_(int2 inputSize_)
{
  return int2(inputSize_.x == 1 ? 1 : (2 | (inputSize_.x & 1)),
               inputSize_.y == 1 ? 1 : (2 | (inputSize_.y & 1)));
}

NVPRO_PYRAMID_TYPE
loadSample_(int2 srcCoord_, int srcLevel_, bool loadFromShared_);

// Handle loading and reducing a rectangle of size kernelSize_
// with the given upper-left coordinate srcCoord_. Samples read from
// mip level srcLevel_ if !loadFromShared_, sharedLevel_ otherwise.
//
// kernelSize_ must range from 1x1 to 3x3.
//
// Once computed, the sample is written to the given coordinate of the
// specified destination mip level, and returned. The destination
// image size is needed to compute the kernel weights.
NVPRO_PYRAMID_TYPE reduceStoreSample_(int2 srcCoord_, int srcLevel_,
                                      bool  loadFromShared_,
                                      int2 kernelSize_,
                                      int2 dstImageSize_,
                                      int2 dstCoord_, int dstLevel_)
{
  bool  lfs_ = loadFromShared_;
  float n_   = dstImageSize_.y;
  float rcp_ = 1.0f / (2 * n_ + 1);
  float w0_  = rcp_ * (n_ - dstCoord_.y);
  float w1_  = rcp_ * n_;
  float w2_  = 1.0f - w0_ - w1_;

  NVPRO_PYRAMID_TYPE v0_, v1_, v2_, h0_, h1_, h2_, out_;

  // Reduce vertically up to 3 times (depending on kernel horizontal size)
  switch (kernelSize_.x)
  {
    case 3:
      switch (kernelSize_.y)
      {
        case 3: v2_ = loadSample_(srcCoord_ + int2(2, 2), srcLevel_, lfs_);
        case 2: v1_ = loadSample_(srcCoord_ + int2(2, 1), srcLevel_, lfs_);
        case 1: v0_ = loadSample_(srcCoord_ + int2(2, 0), srcLevel_, lfs_);
      }
      switch (kernelSize_.y)
      {
        case 3: NVPRO_PYRAMID_REDUCE(w0_, v0_, w1_, v1_, w2_, v2_, h2_); break;
        case 2: NVPRO_PYRAMID_REDUCE2(v0_, v1_, h2_); break;
        case 1: h2_ = v0_; break;
      }
      // fallthru
    case 2:
      switch (kernelSize_.y)
      {
        case 3: v2_ = loadSample_(srcCoord_ + int2(1, 2), srcLevel_, lfs_);
        case 2: v1_ = loadSample_(srcCoord_ + int2(1, 1), srcLevel_, lfs_);
        case 1: v0_ = loadSample_(srcCoord_ + int2(1, 0), srcLevel_, lfs_);
      }
      switch (kernelSize_.y)
      {
        case 3: NVPRO_PYRAMID_REDUCE(w0_, v0_, w1_, v1_, w2_, v2_, h1_); break;
        case 2: NVPRO_PYRAMID_REDUCE2(v0_, v1_, h1_); break;
        case 1: h1_ = v0_; break;
      }
    case 1:
      switch (kernelSize_.y)
      {
        case 3: v2_ = loadSample_(srcCoord_ + int2(0, 2), srcLevel_, lfs_);
        case 2: v1_ = loadSample_(srcCoord_ + int2(0, 1), srcLevel_, lfs_);
        case 1: v0_ = loadSample_(srcCoord_ + int2(0, 0), srcLevel_, lfs_);
      }
      switch (kernelSize_.y)
      {
        case 3: NVPRO_PYRAMID_REDUCE(w0_, v0_, w1_, v1_, w2_, v2_, h0_); break;
        case 2: NVPRO_PYRAMID_REDUCE2(v0_, v1_, h0_); break;
        case 1: h0_ = v0_; break;
      }
  }

  // Reduce up to 3 samples horizontally.
  switch (kernelSize_.x)
  {
    case 3:
      n_   = dstImageSize_.x;
      rcp_ = 1.0f / (2 * n_ + 1);
      w0_  = rcp_ * (n_ - dstCoord_.x);
      w1_  = rcp_ * n_;
      w2_  = 1.0f - w0_ - w1_;
      NVPRO_PYRAMID_REDUCE(w0_, h0_, w1_, h1_, w2_, h2_, out_);
      break;
    case 2:
      NVPRO_PYRAMID_REDUCE2(h0_, h1_, out_);
      break;
    case 1:
      out_ = h0_;
  }

  // Write out sample.
  NVPRO_PYRAMID_STORE(dstCoord_, dstLevel_, out_);
  return out_;
}

NVPRO_PYRAMID_TYPE
loadSample_(int2 srcCoord_, int srcLevel_, bool loadFromShared_)
{
  NVPRO_PYRAMID_TYPE loaded_;
  if (loadFromShared_)
  {
    NVPRO_PYRAMID_SHARED_LOAD((sharedLevel_[srcCoord_.y][srcCoord_.x]), loaded_);
  }
  else
  {
    NVPRO_PYRAMID_LOAD(srcCoord_, srcLevel_, loaded_);
  }
  return loaded_;
}



// Compute and write out (to the 1st mip level generated) the samples
// at coordinates
//     initDstCoord_,
//     initDstCoord_ + step_, ...
//     initDstCoord_ + (iterations_-1) * step_
// and cache them at in the sharedLevel_ tile at coordinates
//     initSharedCoord_,
//     initSharedCoord_ + step_, ...
//     initSharedCoord_ + (iterations_-1) * step_
// If boundsCheck_ is true, skip coordinates that are out of bounds.
void intermediateLevelLoop_(int2 initDstCoord_,
                            int2 initSharedCoord_,
                            int2 step_,
                            int   iterations_,
                            bool  boundsCheck_)
{
  int2 dstCoord_     = initDstCoord_;
  int2 sharedCoord_  = initSharedCoord_;
  int   srcLevel_     = int(NVPRO_PYRAMID_INPUT_LEVEL_);
  int   dstLevel_     = srcLevel_ + 1;
  int2 srcImageSize_ = NVPRO_PYRAMID_LEVEL_SIZE(srcLevel_);
  int2 dstImageSize_ = NVPRO_PYRAMID_LEVEL_SIZE(dstLevel_);
  int2 kernelSize_   = kernelSizeFromInputSize_(srcImageSize_);

  for (int i_ = 0; i_ < iterations_; ++i_)
  {
    int2 srcCoord_ = dstCoord_ * 2;

    // Optional bounds check.
    if (boundsCheck_)
    {
      if (uint(dstCoord_.x) >= uint(dstImageSize_.x)) continue;
      if (uint(dstCoord_.y) >= uint(dstImageSize_.y)) continue;
    }

    bool loadFromShared_ = false;
    NVPRO_PYRAMID_TYPE sample_ =
        reduceStoreSample_(srcCoord_, srcLevel_, loadFromShared_, kernelSize_,
                           dstImageSize_, dstCoord_, dstLevel_);

    // Above function handles writing to the actual output; manually
    // cache into shared memory here.
    NVPRO_PYRAMID_SHARED_STORE((sharedLevel_[sharedCoord_.y][sharedCoord_.x]),
                               sample_);
    dstCoord_ += step_;
    sharedCoord_ += step_;
  }
}

// Function for the workgroup that handles filling the intermediate level
// (caching it in shared memory as well).
//
// We need somewhere from 16x16 to 17x17 samples, depending
// on what the kernel size for the 2nd mip level generation will be.
//
// dstTileCoord_ : upper left coordinate of the tile to generate.
// boundsCheck_  : whether to skip samples that are out-of-bounds.
void fillIntermediateTile_(int2 dstTileCoord_, bool boundsCheck_, uint localInvocationIndex : SV_GroupIndex)
{
  uint localIdx_ = int(localInvocationIndex);

  int2 initThreadOffset_;
  int2 step_;
  int   iterations_;

  int2 dstImageSize_ =
      NVPRO_PYRAMID_LEVEL_SIZE((int(NVPRO_PYRAMID_INPUT_LEVEL_) + 1));
  int2 futureKernelSize_ = kernelSizeFromInputSize_(dstImageSize_);

  if (futureKernelSize_.x == 3)
  {
    if (futureKernelSize_.y == 3)
    {
      // Fill in 2 17x7 steps and 1 17x3 step (9 idle threads)
      initThreadOffset_ = int2(localIdx_ % 17u, localIdx_ / 17u);
      step_             = int2(0, 7);
      iterations_       = localIdx_ >= 7 * 17 ? 0 : localIdx_ < 3 * 17 ? 3 : 2;
    }
    else  // Future 3x[2,1] kernel
    {
      // Fill in 2 8x16 steps and 1 1x16 step
      initThreadOffset_ = int2(localIdx_ / 16u, localIdx_ % 16u);
      step_             = int2(8, 0);
      iterations_       = localIdx_ < 1 * 16 ? 3 : 2;
    }
  }
  else
  {
    if (futureKernelSize_.y == 3)
    {
      // Fill in 2 16x8 steps and 1 16x1 step
      initThreadOffset_ = int2(localIdx_ % 16u, localIdx_ / 16u);
      step_             = int2(0, 8);
      iterations_       = localIdx_ < 1 * 16 ? 3 : 2;
    }
    else
    {
      // Fill in 2 16x8 steps
      initThreadOffset_ = int2(localIdx_ % 16u, localIdx_ / 16u);
      step_             = int2(0, 8);
      iterations_       = 2;
    }
  }

  intermediateLevelLoop_(dstTileCoord_ + initThreadOffset_, initThreadOffset_,
                         step_, iterations_, boundsCheck_);
}



// Function for the workgroup that handles filling the last level tile
// (2nd level after the original input level), using as input the
// tile in shared memory.
//
// dstTileCoord_ : upper left coordinate of the tile to generate.
// boundsCheck_  : whether to skip samples that are out-of-bounds.
void fillLastTile_(int2 dstTileCoord_, bool boundsCheck_, uint localInvocationIndex : SV_GroupIndex)
{
  uint localIdx_ = localInvocationIndex;

  if (localIdx_ < 8 * 8)
  {
    int2 threadOffset_ = int2(localIdx_ % 8u, localIdx_ / 8u);
    int   srcLevel_     = int(NVPRO_PYRAMID_INPUT_LEVEL_) + 1;
    int   dstLevel_     = int(NVPRO_PYRAMID_INPUT_LEVEL_) + 2;
    int2 srcImageSize_ = NVPRO_PYRAMID_LEVEL_SIZE(srcLevel_);
    int2 dstImageSize_ = NVPRO_PYRAMID_LEVEL_SIZE(dstLevel_);

    int2 srcSharedCoord_ = threadOffset_ * 2;
    bool loadFromShared_  = true;
    int2 kernelSize_     = kernelSizeFromInputSize_(srcImageSize_);
    int2 dstCoord_       = threadOffset_ + dstTileCoord_;

    bool inBounds_ = true;
    if (boundsCheck_)
    {
      inBounds_ = (uint(dstCoord_.x) < uint(dstImageSize_.x))
                  && (uint(dstCoord_.y) < uint(dstImageSize_.y));
    }
    if (inBounds_)
    {
      reduceStoreSample_(srcSharedCoord_, 0, loadFromShared_, kernelSize_,
                         dstImageSize_, dstCoord_, dstLevel_);
    }
  }
}



void nvproPyramidMain(uint2 globalInvocationId : SV_DispatchThreadID, uint localInvocationIndex : SV_GroupIndex,
    uint3 workgroupId : SV_GroupID)
{
  int inputLevel_ = int(NVPRO_PYRAMID_INPUT_LEVEL_);

  if (NVPRO_PYRAMID_LEVEL_COUNT_ == 1u)
  {
    int2 kernelSize_ =
        kernelSizeFromInputSize_(NVPRO_PYRAMID_LEVEL_SIZE(inputLevel_));
    int2 dstImageSize_ = NVPRO_PYRAMID_LEVEL_SIZE((inputLevel_ + 1));
    int2 dstCoord_     = int2(int(globalInvocationId.x) % dstImageSize_.x,
                                int(globalInvocationId.x) / dstImageSize_.x);
    int2 srcCoord_ = dstCoord_ * 2;

    if (dstCoord_.y < dstImageSize_.y)
    {
      reduceStoreSample_(srcCoord_, inputLevel_, false, kernelSize_,
                         dstImageSize_, dstCoord_, inputLevel_ + 1);
    }
  }
  else  // Handling two levels.
  {
    // Assign a 8x8 tile of mip level inputLevel_ + 2 to this workgroup.
    int   level2_     = inputLevel_ + 2;
    int2 level2Size_ = NVPRO_PYRAMID_LEVEL_SIZE(level2_);
    int2 tileCount_;
    tileCount_.x   = int(uint(level2Size_.x + 7) / 8u);
    tileCount_.y   = int(uint(level2Size_.y + 7) / 8u);
    int2 tileIdx_ = int2(workgroupId.x % uint(tileCount_.x),
                           workgroupId.x / uint(tileCount_.x));
    uint localIdx_ = localInvocationIndex;

    // Determine if bounds checking is needed; this is only the case
    // for tiles at the right or bottom fringe that might be cut off
    // by the image border. Note that later, I use if statements rather
    // than passing boundsCheck_ directly to convince the compiler
    // to inline everything.
    bool boundsCheck_ = tileIdx_.x >= tileCount_.x - 1 ||
                        tileIdx_.y >= tileCount_.y - 1;

    if (boundsCheck_)
    {
      // Compute the tile in level inputLevel_ + 1 that's needed to
      // compute the above 8x8 tile.
      fillIntermediateTile_(tileIdx_ * 2 * int2(8, 8), true, localInvocationIndex);
      GroupMemoryBarrierWithGroupSync();

      // Compute the inputLevel_ + 2 tile of size 8x8, loading
      // inupts from shared memory.
      fillLastTile_(tileIdx_ * int2(8, 8), true, localInvocationIndex);
    }
    else
    {
      // Same with no bounds checking.
      fillIntermediateTile_(tileIdx_ * 2 * int2(8, 8), false, localInvocationIndex);
      GroupMemoryBarrierWithGroupSync();
      fillLastTile_(tileIdx_ * int2(8, 8), false, localInvocationIndex);
    }
  }
}

#endif /* !NVPRO_PYRAMID_IS_FAST_PIPELINE */

#undef NVPRO_PYRAMID_2D_REDUCE_
#undef NVPRO_PYRAMID_LEVEL_COUNT_
#undef NVPRO_PYRAMID_INPUT_LEVEL_
