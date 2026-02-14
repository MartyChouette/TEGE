#include "Enjin/Renderer/SDFRenderer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>

namespace Enjin {
namespace Renderer {

// ===========================================================================
// Marching cubes lookup tables (Paul Bourke / public domain)
// ===========================================================================

static const u16 s_MCEdgeTable[256] = {
    0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
};

static const i32 s_MCTriTable[256][16] = {
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 1, 9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 8, 3, 9, 8, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 3, 1, 2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 9, 2,10, 0, 2, 9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 2, 8, 3, 2,10, 8,10, 9, 8,-1,-1,-1,-1,-1,-1,-1},
    { 3,11, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0,11, 2, 8,11, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 9, 0, 2, 3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1,11, 2, 1, 9,11, 9, 8,11,-1,-1,-1,-1,-1,-1,-1},
    { 3,10, 1,11,10, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0,10, 1, 0, 8,10, 8,11,10,-1,-1,-1,-1,-1,-1,-1},
    { 3, 9, 0, 3,11, 9,11,10, 9,-1,-1,-1,-1,-1,-1,-1},
    { 9, 8,10,10, 8,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 7, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 3, 0, 7, 3, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 1, 9, 8, 4, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 1, 9, 4, 7, 1, 7, 3, 1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,10, 8, 4, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 3, 4, 7, 3, 0, 4, 1, 2,10,-1,-1,-1,-1,-1,-1,-1},
    { 9, 2,10, 9, 0, 2, 8, 4, 7,-1,-1,-1,-1,-1,-1,-1},
    { 2,10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4,-1,-1,-1,-1},
    { 8, 4, 7, 3,11, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11, 4, 7,11, 2, 4, 2, 0, 4,-1,-1,-1,-1,-1,-1,-1},
    { 9, 0, 1, 8, 4, 7, 2, 3,11,-1,-1,-1,-1,-1,-1,-1},
    { 4, 7,11, 9, 4,11, 9,11, 2, 9, 2, 1,-1,-1,-1,-1},
    { 3,10, 1, 3,11,10, 7, 8, 4,-1,-1,-1,-1,-1,-1,-1},
    { 1,11,10, 1, 4,11, 1, 0, 4, 7,11, 4,-1,-1,-1,-1},
    { 4, 7, 8, 9, 0,11, 9,11,10,11, 0, 3,-1,-1,-1,-1},
    { 4, 7,11, 4,11, 9, 9,11,10,-1,-1,-1,-1,-1,-1,-1},
    { 9, 5, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 9, 5, 4, 0, 8, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 5, 4, 1, 5, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 8, 5, 4, 8, 3, 5, 3, 1, 5,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,10, 9, 5, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 3, 0, 8, 1, 2,10, 4, 9, 5,-1,-1,-1,-1,-1,-1,-1},
    { 5, 2,10, 5, 4, 2, 4, 0, 2,-1,-1,-1,-1,-1,-1,-1},
    { 2,10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8,-1,-1,-1,-1},
    { 9, 5, 4, 2, 3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0,11, 2, 0, 8,11, 4, 9, 5,-1,-1,-1,-1,-1,-1,-1},
    { 0, 5, 4, 0, 1, 5, 2, 3,11,-1,-1,-1,-1,-1,-1,-1},
    { 2, 1, 5, 2, 5, 8, 2, 8,11, 4, 8, 5,-1,-1,-1,-1},
    {10, 3,11,10, 1, 3, 9, 5, 4,-1,-1,-1,-1,-1,-1,-1},
    { 4, 9, 5, 0, 8, 1, 8,10, 1, 8,11,10,-1,-1,-1,-1},
    { 5, 4, 0, 5, 0,11, 5,11,10,11, 0, 3,-1,-1,-1,-1},
    { 5, 4, 8, 5, 8,10,10, 8,11,-1,-1,-1,-1,-1,-1,-1},
    { 9, 7, 8, 5, 7, 9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 9, 3, 0, 9, 5, 3, 5, 7, 3,-1,-1,-1,-1,-1,-1,-1},
    { 0, 7, 8, 0, 1, 7, 1, 5, 7,-1,-1,-1,-1,-1,-1,-1},
    { 1, 5, 3, 3, 5, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 9, 7, 8, 9, 5, 7,10, 1, 2,-1,-1,-1,-1,-1,-1,-1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3,-1,-1,-1,-1},
    { 8, 0, 2, 8, 2, 5, 8, 5, 7,10, 5, 2,-1,-1,-1,-1},
    { 2,10, 5, 2, 5, 3, 3, 5, 7,-1,-1,-1,-1,-1,-1,-1},
    { 7, 9, 5, 7, 8, 9, 3,11, 2,-1,-1,-1,-1,-1,-1,-1},
    { 9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7,11,-1,-1,-1,-1},
    { 2, 3,11, 0, 1, 8, 1, 7, 8, 1, 5, 7,-1,-1,-1,-1},
    {11, 2, 1,11, 1, 7, 7, 1, 5,-1,-1,-1,-1,-1,-1,-1},
    { 9, 5, 8, 8, 5, 7,10, 1, 3,10, 3,11,-1,-1,-1,-1},
    { 5, 7, 0, 5, 0, 9, 7,11, 0, 1, 0,10,11,10, 0,-1},
    {11,10, 0,11, 0, 3,10, 5, 0, 8, 0, 7, 5, 7, 0,-1},
    {11,10, 5, 7,11, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {10, 6, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 3, 5,10, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 9, 0, 1, 5,10, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 8, 3, 1, 9, 8, 5,10, 6,-1,-1,-1,-1,-1,-1,-1},
    { 1, 6, 5, 2, 6, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 6, 5, 1, 2, 6, 3, 0, 8,-1,-1,-1,-1,-1,-1,-1},
    { 9, 6, 5, 9, 0, 6, 0, 2, 6,-1,-1,-1,-1,-1,-1,-1},
    { 5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8,-1,-1,-1,-1},
    { 2, 3,11,10, 6, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11, 0, 8,11, 2, 0,10, 6, 5,-1,-1,-1,-1,-1,-1,-1},
    { 0, 1, 9, 2, 3,11, 5,10, 6,-1,-1,-1,-1,-1,-1,-1},
    { 5,10, 6, 1, 9, 2, 9,11, 2, 9, 8,11,-1,-1,-1,-1},
    { 6, 3,11, 6, 5, 3, 5, 1, 3,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8,11, 0,11, 5, 0, 5, 1, 5,11, 6,-1,-1,-1,-1},
    { 3,11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9,-1,-1,-1,-1},
    { 6, 5, 9, 6, 9,11,11, 9, 8,-1,-1,-1,-1,-1,-1,-1},
    { 5,10, 6, 4, 7, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 3, 0, 4, 7, 3, 6, 5,10,-1,-1,-1,-1,-1,-1,-1},
    { 1, 9, 0, 5,10, 6, 8, 4, 7,-1,-1,-1,-1,-1,-1,-1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4,-1,-1,-1,-1},
    { 6, 1, 2, 6, 5, 1, 4, 7, 8,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7,-1,-1,-1,-1},
    { 8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6,-1,-1,-1,-1},
    { 7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9,-1},
    { 3,11, 2, 7, 8, 4,10, 6, 5,-1,-1,-1,-1,-1,-1,-1},
    { 5,10, 6, 4, 7, 2, 4, 2, 0, 2, 7,11,-1,-1,-1,-1},
    { 0, 1, 9, 4, 7, 8, 2, 3,11, 5,10, 6,-1,-1,-1,-1},
    { 9, 2, 1, 9,11, 2, 9, 4,11, 7,11, 4, 5,10, 6,-1},
    { 8, 4, 7, 3,11, 5, 3, 5, 1, 5,11, 6,-1,-1,-1,-1},
    { 5, 1,11, 5,11, 6, 1, 0,11, 7,11, 4, 0, 4,11,-1},
    { 0, 5, 9, 0, 6, 5, 0, 3, 6,11, 6, 3, 8, 4, 7,-1},
    { 6, 5, 9, 6, 9,11, 4, 7, 9, 7,11, 9,-1,-1,-1,-1},
    {10, 4, 9, 6, 4,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4,10, 6, 4, 9,10, 0, 8, 3,-1,-1,-1,-1,-1,-1,-1},
    {10, 0, 1,10, 6, 0, 6, 4, 0,-1,-1,-1,-1,-1,-1,-1},
    { 8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1,10,-1,-1,-1,-1},
    { 1, 4, 9, 1, 2, 4, 2, 6, 4,-1,-1,-1,-1,-1,-1,-1},
    { 3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4,-1,-1,-1,-1},
    { 0, 2, 4, 4, 2, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 8, 3, 2, 8, 2, 4, 4, 2, 6,-1,-1,-1,-1,-1,-1,-1},
    {10, 4, 9,10, 6, 4,11, 2, 3,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 2, 2, 8,11, 4, 9,10, 4,10, 6,-1,-1,-1,-1},
    { 3,11, 2, 0, 1, 6, 0, 6, 4, 6, 1,10,-1,-1,-1,-1},
    { 6, 4, 1, 6, 1,10, 4, 8, 1, 2, 1,11, 8,11, 1,-1},
    { 9, 6, 4, 9, 3, 6, 9, 1, 3,11, 6, 3,-1,-1,-1,-1},
    { 8,11, 1, 8, 1, 0,11, 6, 1, 9, 1, 4, 6, 4, 1,-1},
    { 3,11, 6, 3, 6, 0, 0, 6, 4,-1,-1,-1,-1,-1,-1,-1},
    { 6, 4, 8,11, 6, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 7,10, 6, 7, 8,10, 8, 9,10,-1,-1,-1,-1,-1,-1,-1},
    { 0, 7, 3, 0,10, 7, 0, 9,10, 6, 7,10,-1,-1,-1,-1},
    {10, 6, 7, 1,10, 7, 1, 7, 8, 1, 8, 0,-1,-1,-1,-1},
    {10, 6, 7,10, 7, 1, 1, 7, 3,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7,-1,-1,-1,-1},
    { 2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9,-1},
    { 7, 8, 0, 7, 0, 6, 6, 0, 2,-1,-1,-1,-1,-1,-1,-1},
    { 7, 3, 2, 6, 7, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 2, 3,11,10, 6, 8,10, 8, 9, 8, 6, 7,-1,-1,-1,-1},
    { 2, 0, 7, 2, 7,11, 0, 9, 7, 6, 7,10, 9,10, 7,-1},
    { 1, 8, 0, 1, 7, 8, 1,10, 7, 6, 7,10, 2, 3,11,-1},
    {11, 2, 1,11, 1, 7,10, 6, 1, 6, 7, 1,-1,-1,-1,-1},
    { 8, 9, 6, 8, 6, 7, 9, 1, 6,11, 6, 3, 1, 3, 6,-1},
    { 0, 9, 1,11, 6, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 7, 8, 0, 7, 0, 6, 3,11, 0,11, 6, 0,-1,-1,-1,-1},
    { 7,11, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 7, 6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 3, 0, 8,11, 7, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 1, 9,11, 7, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 8, 1, 9, 8, 3, 1,11, 7, 6,-1,-1,-1,-1,-1,-1,-1},
    {10, 1, 2, 6,11, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,10, 3, 0, 8, 6,11, 7,-1,-1,-1,-1,-1,-1,-1},
    { 2, 9, 0, 2,10, 9, 6,11, 7,-1,-1,-1,-1,-1,-1,-1},
    { 6,11, 7, 2,10, 3,10, 8, 3,10, 9, 8,-1,-1,-1,-1},
    { 7, 2, 3, 6, 2, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 7, 0, 8, 7, 6, 0, 6, 2, 0,-1,-1,-1,-1,-1,-1,-1},
    { 2, 7, 6, 2, 3, 7, 0, 1, 9,-1,-1,-1,-1,-1,-1,-1},
    { 1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6,-1,-1,-1,-1},
    {10, 7, 6,10, 1, 7, 1, 3, 7,-1,-1,-1,-1,-1,-1,-1},
    {10, 7, 6, 1, 7,10, 1, 8, 7, 1, 0, 8,-1,-1,-1,-1},
    { 0, 3, 7, 0, 7,10, 0,10, 9, 6,10, 7,-1,-1,-1,-1},
    { 7, 6,10, 7,10, 8, 8,10, 9,-1,-1,-1,-1,-1,-1,-1},
    { 6, 8, 4,11, 8, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 3, 6,11, 3, 0, 6, 0, 4, 6,-1,-1,-1,-1,-1,-1,-1},
    { 8, 6,11, 8, 4, 6, 9, 0, 1,-1,-1,-1,-1,-1,-1,-1},
    { 9, 4, 6, 9, 6, 3, 9, 3, 1,11, 3, 6,-1,-1,-1,-1},
    { 6, 8, 4, 6,11, 8, 2,10, 1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,10, 3, 0,11, 0, 6,11, 0, 4, 6,-1,-1,-1,-1},
    { 4,11, 8, 4, 6,11, 0, 2, 9, 2,10, 9,-1,-1,-1,-1},
    {10, 9, 3,10, 3, 2, 9, 4, 3,11, 3, 6, 4, 6, 3,-1},
    { 8, 2, 3, 8, 4, 2, 4, 6, 2,-1,-1,-1,-1,-1,-1,-1},
    { 0, 4, 2, 4, 6, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8,-1,-1,-1,-1},
    { 1, 9, 4, 1, 4, 2, 2, 4, 6,-1,-1,-1,-1,-1,-1,-1},
    { 8, 1, 3, 8, 6, 1, 8, 4, 6, 6,10, 1,-1,-1,-1,-1},
    {10, 1, 0,10, 0, 6, 6, 0, 4,-1,-1,-1,-1,-1,-1,-1},
    { 4, 6, 3, 4, 3, 8, 6,10, 3, 0, 3, 9,10, 9, 3,-1},
    {10, 9, 4, 6,10, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 9, 5, 7, 6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 3, 4, 9, 5,11, 7, 6,-1,-1,-1,-1,-1,-1,-1},
    { 5, 0, 1, 5, 4, 0, 7, 6,11,-1,-1,-1,-1,-1,-1,-1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5,-1,-1,-1,-1},
    { 9, 5, 4,10, 1, 2, 7, 6,11,-1,-1,-1,-1,-1,-1,-1},
    { 6,11, 7, 1, 2,10, 0, 8, 3, 4, 9, 5,-1,-1,-1,-1},
    { 7, 6,11, 5, 4,10, 4, 2,10, 4, 0, 2,-1,-1,-1,-1},
    { 3, 4, 8, 3, 5, 4, 3, 2, 5,10, 5, 2,11, 7, 6,-1},
    { 7, 2, 3, 7, 6, 2, 5, 4, 9,-1,-1,-1,-1,-1,-1,-1},
    { 9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7,-1,-1,-1,-1},
    { 3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0,-1,-1,-1,-1},
    { 6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8,-1},
    { 9, 5, 4,10, 1, 6, 1, 7, 6, 1, 3, 7,-1,-1,-1,-1},
    { 1, 6,10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4,-1},
    { 4, 0,10, 4,10, 5, 0, 3,10, 6,10, 7, 3, 7,10,-1},
    { 7, 6,10, 7,10, 8, 5, 4,10, 4, 8,10,-1,-1,-1,-1},
    { 6, 9, 5, 6,11, 9,11, 8, 9,-1,-1,-1,-1,-1,-1,-1},
    { 3, 6,11, 0, 6, 3, 0, 5, 6, 0, 9, 5,-1,-1,-1,-1},
    { 0,11, 8, 0, 5,11, 0, 1, 5, 5, 6,11,-1,-1,-1,-1},
    { 6,11, 3, 6, 3, 5, 5, 3, 1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,10, 9, 5,11, 9,11, 8,11, 5, 6,-1,-1,-1,-1},
    { 0,11, 3, 0, 6,11, 0, 9, 6, 5, 6, 9, 1, 2,10,-1},
    {11, 8, 5,11, 5, 6, 8, 0, 5,10, 5, 2, 0, 2, 5,-1},
    { 6,11, 3, 6, 3, 5, 2,10, 3,10, 5, 3,-1,-1,-1,-1},
    { 5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2,-1,-1,-1,-1},
    { 9, 5, 6, 9, 6, 0, 0, 6, 2,-1,-1,-1,-1,-1,-1,-1},
    { 1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8,-1},
    { 1, 5, 6, 2, 1, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 3, 6, 1, 6,10, 3, 8, 6, 5, 6, 9, 8, 9, 6,-1},
    {10, 1, 0,10, 0, 6, 9, 5, 0, 5, 6, 0,-1,-1,-1,-1},
    { 0, 3, 8, 5, 6,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {10, 5, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11, 5,10, 7, 5,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11, 5,10,11, 7, 5, 8, 3, 0,-1,-1,-1,-1,-1,-1,-1},
    { 5,11, 7, 5,10,11, 1, 9, 0,-1,-1,-1,-1,-1,-1,-1},
    {10, 7, 5,10,11, 7, 9, 8, 1, 8, 3, 1,-1,-1,-1,-1},
    {11, 1, 2,11, 7, 1, 7, 5, 1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2,11,-1,-1,-1,-1},
    { 9, 7, 5, 9, 2, 7, 9, 0, 2, 2,11, 7,-1,-1,-1,-1},
    { 7, 5, 2, 7, 2,11, 5, 9, 2, 3, 2, 8, 9, 8, 2,-1},
    { 2, 5,10, 2, 3, 5, 3, 7, 5,-1,-1,-1,-1,-1,-1,-1},
    { 8, 2, 0, 8, 5, 2, 8, 7, 5,10, 2, 5,-1,-1,-1,-1},
    { 9, 0, 1, 5,10, 3, 5, 3, 7, 3,10, 2,-1,-1,-1,-1},
    { 9, 8, 2, 9, 2, 1, 8, 7, 2,10, 2, 5, 7, 5, 2,-1},
    { 1, 3, 5, 3, 7, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 7, 0, 7, 1, 1, 7, 5,-1,-1,-1,-1,-1,-1,-1},
    { 9, 0, 3, 9, 3, 5, 5, 3, 7,-1,-1,-1,-1,-1,-1,-1},
    { 9, 8, 7, 5, 9, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 5, 8, 4, 5,10, 8,10,11, 8,-1,-1,-1,-1,-1,-1,-1},
    { 5, 0, 4, 5,11, 0, 5,10,11,11, 3, 0,-1,-1,-1,-1},
    { 0, 1, 9, 8, 4,10, 8,10,11,10, 4, 5,-1,-1,-1,-1},
    {10,11, 4,10, 4, 5,11, 3, 4, 9, 4, 1, 3, 1, 4,-1},
    { 2, 5, 1, 2, 8, 5, 2,11, 8, 4, 5, 8,-1,-1,-1,-1},
    { 0, 4,11, 0,11, 3, 4, 5,11, 2,11, 1, 5, 1,11,-1},
    { 0, 2, 5, 0, 5, 9, 2,11, 5, 4, 5, 8,11, 8, 5,-1},
    { 9, 4, 5, 2,11, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 2, 5,10, 3, 5, 2, 3, 4, 5, 3, 8, 4,-1,-1,-1,-1},
    { 5,10, 2, 5, 2, 4, 4, 2, 0,-1,-1,-1,-1,-1,-1,-1},
    { 3,10, 2, 3, 5,10, 3, 8, 5, 4, 5, 8, 0, 1, 9,-1},
    { 5,10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2,-1,-1,-1,-1},
    { 8, 4, 5, 8, 5, 3, 3, 5, 1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 4, 5, 1, 0, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5,-1,-1,-1,-1},
    { 9, 4, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4,11, 7, 4, 9,11, 9,10,11,-1,-1,-1,-1,-1,-1,-1},
    { 0, 8, 3, 4, 9, 7, 9,11, 7, 9,10,11,-1,-1,-1,-1},
    { 1,10,11, 1,11, 4, 1, 4, 0, 7, 4,11,-1,-1,-1,-1},
    { 3, 1, 4, 3, 4, 8, 1,10, 4, 7, 4,11,10,11, 4,-1},
    { 4,11, 7, 9,11, 4, 9, 2,11, 9, 1, 2,-1,-1,-1,-1},
    { 9, 7, 4, 9,11, 7, 9, 1,11, 2,11, 1, 0, 8, 3,-1},
    {11, 7, 4,11, 4, 2, 2, 4, 0,-1,-1,-1,-1,-1,-1,-1},
    {11, 7, 4,11, 4, 2, 8, 3, 4, 3, 2, 4,-1,-1,-1,-1},
    { 2, 9,10, 2, 7, 9, 2, 3, 7, 7, 4, 9,-1,-1,-1,-1},
    { 9,10, 7, 9, 7, 4,10, 2, 7, 8, 7, 0, 2, 0, 7,-1},
    { 3, 7,10, 3,10, 2, 7, 4,10, 1,10, 0, 4, 0,10,-1},
    { 1,10, 2, 8, 7, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 9, 1, 4, 1, 7, 7, 1, 3,-1,-1,-1,-1,-1,-1,-1},
    { 4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1,-1,-1,-1,-1},
    { 4, 0, 3, 7, 4, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 4, 8, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 9,10, 8,10,11, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 3, 0, 9, 3, 9,11,11, 9,10,-1,-1,-1,-1,-1,-1,-1},
    { 0, 1,10, 0,10, 8, 8,10,11,-1,-1,-1,-1,-1,-1,-1},
    { 3, 1,10,11, 3,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 2,11, 1,11, 9, 9,11, 8,-1,-1,-1,-1,-1,-1,-1},
    { 3, 0, 9, 3, 9,11, 1, 2, 9, 2,11, 9,-1,-1,-1,-1},
    { 0, 2,11, 8, 0,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 3, 2,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 2, 3, 8, 2, 8,10,10, 8, 9,-1,-1,-1,-1,-1,-1,-1},
    { 9,10, 2, 0, 9, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 2, 3, 8, 2, 8,10, 0, 1, 8, 1,10, 8,-1,-1,-1,-1},
    { 1,10, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 1, 3, 8, 9, 1, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 9, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    { 0, 3, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
};

// ===========================================================================
// SDFVolume
// ===========================================================================

f32 SDFVolume::Sample(i32 x, i32 y, i32 z) const {
    x = std::clamp(x, 0, resX - 1);
    y = std::clamp(y, 0, resY - 1);
    z = std::clamp(z, 0, resZ - 1);
    return data[static_cast<usize>(z) * resY * resX + static_cast<usize>(y) * resX + x];
}

// ===========================================================================
// MeshToSDF
// ===========================================================================

f32 MeshToSDF::PointTriangleDistSq(const Math::Vector3& p,
                                     const Math::Vector3& a,
                                     const Math::Vector3& b,
                                     const Math::Vector3& c,
                                     Math::Vector3& closestPoint) {
    // Compute vectors
    Math::Vector3 ab = b - a;
    Math::Vector3 ac = c - a;
    Math::Vector3 ap = p - a;

    f32 d1 = ab.Dot(ap);
    f32 d2 = ac.Dot(ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        closestPoint = a;
        return ap.LengthSquared();
    }

    Math::Vector3 bp = p - b;
    f32 d3 = ab.Dot(bp);
    f32 d4 = ac.Dot(bp);
    if (d3 >= 0.0f && d4 <= d3) {
        closestPoint = b;
        return bp.LengthSquared();
    }

    f32 vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        f32 v = d1 / (d1 - d3);
        closestPoint = a + ab * v;
        Math::Vector3 diff = p - closestPoint;
        return diff.LengthSquared();
    }

    Math::Vector3 cp = p - c;
    f32 d5 = ab.Dot(cp);
    f32 d6 = ac.Dot(cp);
    if (d6 >= 0.0f && d5 <= d6) {
        closestPoint = c;
        return cp.LengthSquared();
    }

    f32 vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        f32 w = d2 / (d2 - d6);
        closestPoint = a + ac * w;
        Math::Vector3 diff = p - closestPoint;
        return diff.LengthSquared();
    }

    f32 va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        f32 w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        closestPoint = b + (c - b) * w;
        Math::Vector3 diff = p - closestPoint;
        return diff.LengthSquared();
    }

    f32 denom = 1.0f / (va + vb + vc);
    f32 v = vb * denom;
    f32 w = vc * denom;
    closestPoint = a + ab * v + ac * w;
    Math::Vector3 diff = p - closestPoint;
    return diff.LengthSquared();
}

f32 MeshToSDF::ComputeSign(const Math::Vector3& p,
                            const Math::Vector3& closestPoint,
                            const Math::Vector3& triNormal) {
    Math::Vector3 displacement = p - closestPoint;
    f32 dot = displacement.Dot(triNormal);
    return dot >= 0.0f ? 1.0f : -1.0f;
}

SDFVolume MeshToSDF::ConvertMesh(const std::vector<ECS::MeshComponent::Vertex>& vertices,
                                  const std::vector<u32>& indices,
                                  i32 resolution) {
    SDFVolume volume;

    if (vertices.empty() || indices.empty() || resolution < 2) return volume;
    if (indices.size() % 3 != 0) return volume;

    // Clamp resolution to something reasonable
    resolution = std::clamp(resolution, 2, 256);

    // Compute mesh AABB
    Math::Vector3 bmin(std::numeric_limits<f32>::max());
    Math::Vector3 bmax(std::numeric_limits<f32>::lowest());

    for (const auto& vert : vertices) {
        bmin.x = std::min(bmin.x, vert.position.x);
        bmin.y = std::min(bmin.y, vert.position.y);
        bmin.z = std::min(bmin.z, vert.position.z);
        bmax.x = std::max(bmax.x, vert.position.x);
        bmax.y = std::max(bmax.y, vert.position.y);
        bmax.z = std::max(bmax.z, vert.position.z);
    }

    // Add padding (10% of diagonal)
    Math::Vector3 diagonal = bmax - bmin;
    f32 padding = diagonal.Length() * 0.1f;
    if (padding < 0.001f) padding = 0.001f;
    bmin -= Math::Vector3(padding);
    bmax += Math::Vector3(padding);

    volume.boundsMin = bmin;
    volume.boundsMax = bmax;
    volume.resX = resolution;
    volume.resY = resolution;
    volume.resZ = resolution;

    Math::Vector3 extent = bmax - bmin;
    volume.voxelSize = std::max({extent.x, extent.y, extent.z}) / static_cast<f32>(resolution - 1);

    usize totalVoxels = static_cast<usize>(resolution) * resolution * resolution;
    volume.data.resize(totalVoxels);

    // Precompute triangle data
    usize triCount = indices.size() / 3;
    struct TriData {
        Math::Vector3 a, b, c;
        Math::Vector3 normal;
    };
    std::vector<TriData> tris(triCount);

    for (usize t = 0; t < triCount; ++t) {
        const auto& va = vertices[indices[t * 3 + 0]].position;
        const auto& vb = vertices[indices[t * 3 + 1]].position;
        const auto& vc = vertices[indices[t * 3 + 2]].position;
        tris[t].a = va;
        tris[t].b = vb;
        tris[t].c = vc;
        Math::Vector3 edge1 = vb - va;
        Math::Vector3 edge2 = vc - va;
        tris[t].normal = edge1.Cross(edge2).Normalized();
    }

    // For each voxel, find closest triangle and compute signed distance
    for (i32 z = 0; z < resolution; ++z) {
        f32 wz = bmin.z + (extent.z * static_cast<f32>(z) / static_cast<f32>(resolution - 1));
        for (i32 y = 0; y < resolution; ++y) {
            f32 wy = bmin.y + (extent.y * static_cast<f32>(y) / static_cast<f32>(resolution - 1));
            for (i32 x = 0; x < resolution; ++x) {
                f32 wx = bmin.x + (extent.x * static_cast<f32>(x) / static_cast<f32>(resolution - 1));
                Math::Vector3 pos(wx, wy, wz);

                f32 minDistSq = std::numeric_limits<f32>::max();
                Math::Vector3 bestClosest;
                Math::Vector3 bestNormal;

                for (usize t = 0; t < triCount; ++t) {
                    Math::Vector3 closest;
                    f32 distSq = PointTriangleDistSq(pos, tris[t].a, tris[t].b, tris[t].c, closest);
                    if (distSq < minDistSq) {
                        minDistSq = distSq;
                        bestClosest = closest;
                        bestNormal = tris[t].normal;
                    }
                }

                f32 dist = std::sqrt(minDistSq);
                f32 sign = ComputeSign(pos, bestClosest, bestNormal);

                usize idx = static_cast<usize>(z) * resolution * resolution
                          + static_cast<usize>(y) * resolution
                          + x;
                volume.data[idx] = sign * dist;
            }
        }
    }

    return volume;
}

f32 MeshToSDF::EvaluateSDF(const SDFVolume& volume, const Math::Vector3& worldPos) {
    if (!volume.IsValid()) return std::numeric_limits<f32>::max();

    Math::Vector3 extent = volume.boundsMax - volume.boundsMin;
    if (extent.x <= 0.0f || extent.y <= 0.0f || extent.z <= 0.0f) {
        return std::numeric_limits<f32>::max();
    }

    // Map world position to grid coordinates
    f32 gx = (worldPos.x - volume.boundsMin.x) / extent.x * static_cast<f32>(volume.resX - 1);
    f32 gy = (worldPos.y - volume.boundsMin.y) / extent.y * static_cast<f32>(volume.resY - 1);
    f32 gz = (worldPos.z - volume.boundsMin.z) / extent.z * static_cast<f32>(volume.resZ - 1);

    // Trilinear interpolation
    i32 x0 = static_cast<i32>(std::floor(gx));
    i32 y0 = static_cast<i32>(std::floor(gy));
    i32 z0 = static_cast<i32>(std::floor(gz));
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;
    i32 z1 = z0 + 1;

    f32 fx = gx - static_cast<f32>(x0);
    f32 fy = gy - static_cast<f32>(y0);
    f32 fz = gz - static_cast<f32>(z0);

    fx = std::clamp(fx, 0.0f, 1.0f);
    fy = std::clamp(fy, 0.0f, 1.0f);
    fz = std::clamp(fz, 0.0f, 1.0f);

    f32 c000 = volume.Sample(x0, y0, z0);
    f32 c100 = volume.Sample(x1, y0, z0);
    f32 c010 = volume.Sample(x0, y1, z0);
    f32 c110 = volume.Sample(x1, y1, z0);
    f32 c001 = volume.Sample(x0, y0, z1);
    f32 c101 = volume.Sample(x1, y0, z1);
    f32 c011 = volume.Sample(x0, y1, z1);
    f32 c111 = volume.Sample(x1, y1, z1);

    f32 c00 = c000 * (1.0f - fx) + c100 * fx;
    f32 c10 = c010 * (1.0f - fx) + c110 * fx;
    f32 c01 = c001 * (1.0f - fx) + c101 * fx;
    f32 c11 = c011 * (1.0f - fx) + c111 * fx;

    f32 c0 = c00 * (1.0f - fy) + c10 * fy;
    f32 c1 = c01 * (1.0f - fy) + c11 * fy;

    return c0 * (1.0f - fz) + c1 * fz;
}

Math::Vector3 MeshToSDF::ComputeNormalSDF(const SDFVolume& volume, const Math::Vector3& worldPos) {
    if (!volume.IsValid()) return Math::Vector3(0.0f, 1.0f, 0.0f);

    f32 eps = volume.voxelSize * 0.5f;
    if (eps < 0.0001f) eps = 0.0001f;

    f32 dx = EvaluateSDF(volume, Math::Vector3(worldPos.x + eps, worldPos.y, worldPos.z))
           - EvaluateSDF(volume, Math::Vector3(worldPos.x - eps, worldPos.y, worldPos.z));
    f32 dy = EvaluateSDF(volume, Math::Vector3(worldPos.x, worldPos.y + eps, worldPos.z))
           - EvaluateSDF(volume, Math::Vector3(worldPos.x, worldPos.y - eps, worldPos.z));
    f32 dz = EvaluateSDF(volume, Math::Vector3(worldPos.x, worldPos.y, worldPos.z + eps))
           - EvaluateSDF(volume, Math::Vector3(worldPos.x, worldPos.y, worldPos.z - eps));

    Math::Vector3 n(dx, dy, dz);
    f32 len = n.Length();
    if (len > Math::EPSILON) {
        n = n / len;
    } else {
        n = Math::Vector3(0.0f, 1.0f, 0.0f);
    }
    return n;
}

// ===========================================================================
// SDFTextRenderer
// ===========================================================================

void SDFTextRenderer::ComputeDistanceField8SSEDT(const std::vector<bool>& inside,
                                                   i32 w, i32 h,
                                                   std::vector<f32>& outDist) {
    // 8SSEDT: 8-point Sequential Signed Euclidean Distance Transform
    // Two-pass sweeping with 8 neighbor offsets per pixel

    usize sz = static_cast<usize>(w) * h;
    std::vector<i32> gdx(sz);
    std::vector<i32> gdy(sz);

    // Initialize: inside pixels = 0, outside = large
    for (usize i = 0; i < sz; ++i) {
        if (inside[i]) {
            gdx[i] = 0;
            gdy[i] = 0;
        } else {
            gdx[i] = w;
            gdy[i] = h;
        }
    }

    auto distFromOff = [](i32 dx, i32 dy) -> f32 {
        return std::sqrt(static_cast<f32>(dx * dx + dy * dy));
    };

    auto tryUpdate = [&](usize di, usize si, i32 offX, i32 offY) {
        i32 cdx = gdx[si] + offX;
        i32 cdy = gdy[si] + offY;
        if (distFromOff(cdx, cdy) < distFromOff(gdx[di], gdy[di])) {
            gdx[di] = cdx;
            gdy[di] = cdy;
        }
    };

    // Forward pass (top-left to bottom-right)
    for (i32 y = 0; y < h; ++y) {
        for (i32 x = 0; x < w; ++x) {
            usize di = static_cast<usize>(y) * w + x;
            if (y > 0) {
                if (x > 0)     tryUpdate(di, static_cast<usize>(y - 1) * w + (x - 1), 1, 1);
                               tryUpdate(di, static_cast<usize>(y - 1) * w + x,        0, 1);
                if (x + 1 < w) tryUpdate(di, static_cast<usize>(y - 1) * w + (x + 1),-1, 1);
            }
            if (x > 0)        tryUpdate(di, static_cast<usize>(y) * w + (x - 1),       1, 0);
        }
        // Right-to-left for horizontal neighbor
        for (i32 rx = w - 1; rx >= 0; --rx) {
            if (rx + 1 < w) {
                usize di = static_cast<usize>(y) * w + rx;
                usize si = static_cast<usize>(y) * w + (rx + 1);
                tryUpdate(di, si, -1, 0);
            }
        }
    }

    // Backward pass (bottom-right to top-left)
    for (i32 y = h - 1; y >= 0; --y) {
        for (i32 bx = w - 1; bx >= 0; --bx) {
            usize di = static_cast<usize>(y) * w + bx;
            if (y + 1 < h) {
                if (bx + 1 < w) tryUpdate(di, static_cast<usize>(y + 1) * w + (bx + 1), -1, -1);
                                tryUpdate(di, static_cast<usize>(y + 1) * w + bx,          0, -1);
                if (bx > 0)    tryUpdate(di, static_cast<usize>(y + 1) * w + (bx - 1),  1, -1);
            }
            if (bx + 1 < w)   tryUpdate(di, static_cast<usize>(y) * w + (bx + 1),       -1,  0);
        }
        // Left-to-right
        for (i32 lx = 0; lx < w; ++lx) {
            if (lx > 0) {
                usize di = static_cast<usize>(y) * w + lx;
                usize si = static_cast<usize>(y) * w + (lx - 1);
                tryUpdate(di, si, 1, 0);
            }
        }
    }

    outDist.resize(sz);
    for (usize i = 0; i < sz; ++i) {
        outDist[i] = distFromOff(gdx[i], gdy[i]);
    }
}

SDFGlyph SDFTextRenderer::GenerateGlyphSDF(const u8* glyphBitmap,
                                             i32 width, i32 height,
                                             i32 padding, f32 spreadRadius) {
    SDFGlyph glyph;
    if (!glyphBitmap || width <= 0 || height <= 0) return glyph;
    if (width > 4096 || height > 4096) return glyph;
    if (padding < 0) padding = 0;
    if (spreadRadius <= 0.0f) spreadRadius = 8.0f;

    i32 paddedW = width + padding * 2;
    i32 paddedH = height + padding * 2;

    usize sz = static_cast<usize>(paddedW) * paddedH;

    // Build inside/outside masks from the alpha-thresholded bitmap
    std::vector<bool> insideMask(sz, false);
    std::vector<bool> outsideMask(sz, true);

    for (i32 y = 0; y < height; ++y) {
        for (i32 x = 0; x < width; ++x) {
            bool opaque = glyphBitmap[static_cast<usize>(y) * width + x] > 127;
            usize pi = static_cast<usize>(y + padding) * paddedW + (x + padding);
            insideMask[pi] = opaque;
            outsideMask[pi] = !opaque;
        }
    }

    // Compute distance fields for inside and outside regions
    std::vector<f32> insideDist, outsideDist;
    ComputeDistanceField8SSEDT(insideMask, paddedW, paddedH, insideDist);
    ComputeDistanceField8SSEDT(outsideMask, paddedW, paddedH, outsideDist);

    // Combine into signed distance field
    glyph.width = paddedW;
    glyph.height = paddedH;
    glyph.sdfData.resize(sz);

    for (usize i = 0; i < sz; ++i) {
        // Negative inside, positive outside
        f32 d = insideMask[i] ? -outsideDist[i] : insideDist[i];
        glyph.sdfData[i] = std::clamp(d, -spreadRadius, spreadRadius);
    }

    glyph.baseline = static_cast<f32>(padding + height);
    glyph.advance = static_cast<f32>(width);

    return glyph;
}

MeshData SDFTextRenderer::RenderText(const std::string& text,
                                      const std::vector<SDFGlyph>& fontSDFAtlas,
                                      const Math::Vector3& position,
                                      f32 fontSize,
                                      const Math::Vector3& color,
                                      const SDFTextConfig& config) {
    MeshData mesh;
    if (text.empty() || fontSDFAtlas.empty()) return mesh;

    f32 cursorX = position.x;
    f32 baseScale = fontSize > 0.0f ? fontSize / 32.0f : 1.0f;  // Assume 32px reference size

    for (usize i = 0; i < text.size(); ++i) {
        u8 charCode = static_cast<u8>(text[i]);
        if (charCode >= fontSDFAtlas.size()) continue;

        const SDFGlyph& glyph = fontSDFAtlas[charCode];
        if (glyph.sdfData.empty() || glyph.width <= 0 || glyph.height <= 0) {
            cursorX += glyph.advance * baseScale;
            continue;
        }

        f32 quadW = static_cast<f32>(glyph.width) * baseScale;
        f32 quadH = static_cast<f32>(glyph.height) * baseScale;
        f32 baseY = position.y - glyph.baseline * baseScale;

        // Quad corners (XY plane, facing +Z)
        u32 baseIdx = static_cast<u32>(mesh.vertices.size());

        ECS::MeshComponent::Vertex v0, v1, v2, v3;

        v0.position = Math::Vector3(cursorX,          baseY + quadH, position.z);
        v0.normal   = Math::Vector3(0.0f, 0.0f, 1.0f);
        v0.uv       = Math::Vector2(0.0f, 0.0f);
        v0.color    = Math::Vector4(color.x, color.y, color.z, 1.0f);

        v1.position = Math::Vector3(cursorX + quadW,  baseY + quadH, position.z);
        v1.normal   = Math::Vector3(0.0f, 0.0f, 1.0f);
        v1.uv       = Math::Vector2(1.0f, 0.0f);
        v1.color    = Math::Vector4(color.x, color.y, color.z, 1.0f);

        v2.position = Math::Vector3(cursorX + quadW,  baseY,          position.z);
        v2.normal   = Math::Vector3(0.0f, 0.0f, 1.0f);
        v2.uv       = Math::Vector2(1.0f, 1.0f);
        v2.color    = Math::Vector4(color.x, color.y, color.z, 1.0f);

        v3.position = Math::Vector3(cursorX,          baseY,          position.z);
        v3.normal   = Math::Vector3(0.0f, 0.0f, 1.0f);
        v3.uv       = Math::Vector2(0.0f, 1.0f);
        v3.color    = Math::Vector4(color.x, color.y, color.z, 1.0f);

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3);

        // Two triangles per quad
        mesh.indices.push_back(baseIdx + 0);
        mesh.indices.push_back(baseIdx + 1);
        mesh.indices.push_back(baseIdx + 2);

        mesh.indices.push_back(baseIdx + 0);
        mesh.indices.push_back(baseIdx + 2);
        mesh.indices.push_back(baseIdx + 3);

        cursorX += glyph.advance * baseScale;
    }

    return mesh;
}

// ===========================================================================
// SDFMeshRenderer
// ===========================================================================

SDFMeshRenderer::TraceResult SDFMeshRenderer::SphereTrace(const SDFVolume& volume,
                                                            const Math::Vector3& rayOrigin,
                                                            const Math::Vector3& rayDir,
                                                            i32 maxSteps,
                                                            f32 epsilon) {
    TraceResult result;
    if (!volume.IsValid()) return result;

    // Normalize direction
    Math::Vector3 dir = rayDir.Normalized();
    if (dir.LengthSquared() < Math::EPSILON) return result;

    // Compute maximum trace distance based on volume diagonal
    Math::Vector3 diag = volume.boundsMax - volume.boundsMin;
    f32 maxDist = diag.Length() * 2.0f;

    Math::Vector3 pos = rayOrigin;
    f32 totalDist = 0.0f;

    for (i32 step = 0; step < maxSteps; ++step) {
        f32 dist = MeshToSDF::EvaluateSDF(volume, pos);

        if (std::abs(dist) < epsilon) {
            result.hit = true;
            result.distance = totalDist;
            result.hitPoint = pos;
            return result;
        }

        // March forward by the SDF distance (safe step)
        f32 marchDist = std::max(std::abs(dist), epsilon * 0.5f);
        pos = pos + dir * marchDist;
        totalDist += marchDist;

        if (totalDist > maxDist) break;
    }

    return result;
}

Math::Vector3 SDFMeshRenderer::InterpolateEdge(const Math::Vector3& p1,
                                                 const Math::Vector3& p2,
                                                 f32 v1, f32 v2, f32 isovalue) {
    f32 denom = v2 - v1;
    if (std::abs(denom) < Math::EPSILON) {
        return (p1 + p2) * 0.5f;
    }
    f32 t = (isovalue - v1) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    return Math::Vector3(
        p1.x + t * (p2.x - p1.x),
        p1.y + t * (p2.y - p1.y),
        p1.z + t * (p2.z - p1.z)
    );
}

MeshData SDFMeshRenderer::ExtractIsosurface(const SDFVolume& volume, f32 isovalue) {
    MeshData mesh;
    if (!volume.IsValid()) return mesh;

    i32 rx = volume.resX;
    i32 ry = volume.resY;
    i32 rz = volume.resZ;

    Math::Vector3 extent = volume.boundsMax - volume.boundsMin;
    f32 stepX = (rx > 1) ? extent.x / static_cast<f32>(rx - 1) : extent.x;
    f32 stepY = (ry > 1) ? extent.y / static_cast<f32>(ry - 1) : extent.y;
    f32 stepZ = (rz > 1) ? extent.z / static_cast<f32>(rz - 1) : extent.z;

    // Iterate over each cell (rx-1 x ry-1 x rz-1)
    for (i32 z = 0; z < rz - 1; ++z) {
        for (i32 y = 0; y < ry - 1; ++y) {
            for (i32 x = 0; x < rx - 1; ++x) {
                // 8 corners of the cube
                f32 val[8];
                val[0] = volume.Sample(x,     y,     z    );
                val[1] = volume.Sample(x + 1, y,     z    );
                val[2] = volume.Sample(x + 1, y + 1, z    );
                val[3] = volume.Sample(x,     y + 1, z    );
                val[4] = volume.Sample(x,     y,     z + 1);
                val[5] = volume.Sample(x + 1, y,     z + 1);
                val[6] = volume.Sample(x + 1, y + 1, z + 1);
                val[7] = volume.Sample(x,     y + 1, z + 1);

                // Determine cube index
                u32 cubeIndex = 0;
                for (i32 i = 0; i < 8; ++i) {
                    if (val[i] < isovalue) cubeIndex |= (1u << i);
                }

                u16 edgeBits = s_MCEdgeTable[cubeIndex];
                if (edgeBits == 0) continue;

                // Corner world positions
                Math::Vector3 corners[8];
                corners[0] = volume.boundsMin + Math::Vector3(static_cast<f32>(x)     * stepX, static_cast<f32>(y)     * stepY, static_cast<f32>(z)     * stepZ);
                corners[1] = volume.boundsMin + Math::Vector3(static_cast<f32>(x + 1) * stepX, static_cast<f32>(y)     * stepY, static_cast<f32>(z)     * stepZ);
                corners[2] = volume.boundsMin + Math::Vector3(static_cast<f32>(x + 1) * stepX, static_cast<f32>(y + 1) * stepY, static_cast<f32>(z)     * stepZ);
                corners[3] = volume.boundsMin + Math::Vector3(static_cast<f32>(x)     * stepX, static_cast<f32>(y + 1) * stepY, static_cast<f32>(z)     * stepZ);
                corners[4] = volume.boundsMin + Math::Vector3(static_cast<f32>(x)     * stepX, static_cast<f32>(y)     * stepY, static_cast<f32>(z + 1) * stepZ);
                corners[5] = volume.boundsMin + Math::Vector3(static_cast<f32>(x + 1) * stepX, static_cast<f32>(y)     * stepY, static_cast<f32>(z + 1) * stepZ);
                corners[6] = volume.boundsMin + Math::Vector3(static_cast<f32>(x + 1) * stepX, static_cast<f32>(y + 1) * stepY, static_cast<f32>(z + 1) * stepZ);
                corners[7] = volume.boundsMin + Math::Vector3(static_cast<f32>(x)     * stepX, static_cast<f32>(y + 1) * stepY, static_cast<f32>(z + 1) * stepZ);

                // Interpolate edge vertices
                // Edge connectivity: edge i connects vertex edgeConn[i][0] to edgeConn[i][1]
                static const i32 edgeConn[12][2] = {
                    {0,1}, {1,2}, {2,3}, {3,0},
                    {4,5}, {5,6}, {6,7}, {7,4},
                    {0,4}, {1,5}, {2,6}, {3,7}
                };

                Math::Vector3 edgeVerts[12];
                for (i32 e = 0; e < 12; ++e) {
                    if (edgeBits & (1 << e)) {
                        i32 c0 = edgeConn[e][0];
                        i32 c1 = edgeConn[e][1];
                        edgeVerts[e] = InterpolateEdge(corners[c0], corners[c1], val[c0], val[c1], isovalue);
                    }
                }

                // Emit triangles from the tri table
                const i32* triRow = s_MCTriTable[cubeIndex];
                for (i32 t = 0; triRow[t] != -1; t += 3) {
                    Math::Vector3 p0 = edgeVerts[triRow[t    ]];
                    Math::Vector3 p1 = edgeVerts[triRow[t + 1]];
                    Math::Vector3 p2 = edgeVerts[triRow[t + 2]];

                    // Compute face normal
                    Math::Vector3 e1 = p1 - p0;
                    Math::Vector3 e2 = p2 - p0;
                    Math::Vector3 normal = e1.Cross(e2).Normalized();

                    // Map SDF distance to UV for material-based rendering
                    u32 baseIdx = static_cast<u32>(mesh.vertices.size());

                    auto makeVert = [&](const Math::Vector3& pos) {
                        ECS::MeshComponent::Vertex v;
                        v.position = pos;
                        v.normal = normal;
                        // Map position in bounding box to UV
                        Math::Vector3 rel = pos - volume.boundsMin;
                        v.uv = Math::Vector2(
                            (extent.x > 0.0f) ? rel.x / extent.x : 0.0f,
                            (extent.y > 0.0f) ? rel.y / extent.y : 0.0f
                        );
                        // Encode SDF gradient magnitude in vertex color alpha
                        f32 sdfVal = MeshToSDF::EvaluateSDF(volume, pos);
                        f32 colorVal = std::clamp(sdfVal * 10.0f + 0.5f, 0.0f, 1.0f);
                        v.color = Math::Vector4(colorVal, colorVal, colorVal, 1.0f);
                        return v;
                    };

                    mesh.vertices.push_back(makeVert(p0));
                    mesh.vertices.push_back(makeVert(p1));
                    mesh.vertices.push_back(makeVert(p2));

                    mesh.indices.push_back(baseIdx + 0);
                    mesh.indices.push_back(baseIdx + 1);
                    mesh.indices.push_back(baseIdx + 2);
                }
            }
        }
    }

    return mesh;
}

MeshData SDFMeshRenderer::ComputeSmoothLOD(const SDFVolume& volume, f32 detailLevel) {
    if (!volume.IsValid()) return {};

    detailLevel = std::clamp(detailLevel, 0.05f, 1.0f);

    // Create a downsampled volume
    i32 newResX = std::max(2, static_cast<i32>(static_cast<f32>(volume.resX) * detailLevel));
    i32 newResY = std::max(2, static_cast<i32>(static_cast<f32>(volume.resY) * detailLevel));
    i32 newResZ = std::max(2, static_cast<i32>(static_cast<f32>(volume.resZ) * detailLevel));

    SDFVolume lodVolume;
    lodVolume.boundsMin = volume.boundsMin;
    lodVolume.boundsMax = volume.boundsMax;
    lodVolume.resX = newResX;
    lodVolume.resY = newResY;
    lodVolume.resZ = newResZ;

    Math::Vector3 extent = volume.boundsMax - volume.boundsMin;
    lodVolume.voxelSize = std::max({extent.x, extent.y, extent.z})
                        / static_cast<f32>(std::max({newResX, newResY, newResZ}) - 1);

    usize totalVoxels = static_cast<usize>(newResX) * newResY * newResZ;
    lodVolume.data.resize(totalVoxels);

    // Sample the original volume at the reduced resolution via trilinear interpolation
    for (i32 z = 0; z < newResZ; ++z) {
        f32 wz = volume.boundsMin.z + extent.z * static_cast<f32>(z) / static_cast<f32>(newResZ - 1);
        for (i32 y = 0; y < newResY; ++y) {
            f32 wy = volume.boundsMin.y + extent.y * static_cast<f32>(y) / static_cast<f32>(newResY - 1);
            for (i32 x = 0; x < newResX; ++x) {
                f32 wx = volume.boundsMin.x + extent.x * static_cast<f32>(x) / static_cast<f32>(newResX - 1);
                usize idx = static_cast<usize>(z) * newResY * newResX
                          + static_cast<usize>(y) * newResX + x;
                lodVolume.data[idx] = MeshToSDF::EvaluateSDF(volume, Math::Vector3(wx, wy, wz));
            }
        }
    }

    return ExtractIsosurface(lodVolume, 0.0f);
}

SDFVolume SDFMeshRenderer::BlendSDFVolumes(const SDFVolume& volumeA,
                                            const SDFVolume& volumeB,
                                            f32 blendFactor) {
    SDFVolume result;

    // Volumes must share the same grid dimensions
    if (!volumeA.IsValid() || !volumeB.IsValid()) return result;
    if (volumeA.resX != volumeB.resX ||
        volumeA.resY != volumeB.resY ||
        volumeA.resZ != volumeB.resZ) return result;

    result.resX = volumeA.resX;
    result.resY = volumeA.resY;
    result.resZ = volumeA.resZ;
    result.boundsMin = volumeA.boundsMin;
    result.boundsMax = volumeA.boundsMax;
    result.voxelSize = volumeA.voxelSize;

    blendFactor = std::clamp(blendFactor, 0.0f, 1.0f);
    f32 invBlend = 1.0f - blendFactor;

    usize total = volumeA.data.size();
    result.data.resize(total);

    // Smooth min blending (analogous to SDFScene::OpSmoothUnion)
    // When blendFactor approaches 0 or 1, this degrades to pure A or B.
    // In between, we use smooth minimum for organic blending.
    f32 k = blendFactor * (1.0f - blendFactor) * 4.0f;  // Peak smoothness at 0.5

    for (usize i = 0; i < total; ++i) {
        f32 a = volumeA.data[i];
        f32 b = volumeB.data[i];

        if (k > 0.001f) {
            // Smooth union with dynamic smoothness
            f32 h = std::max(k - std::abs(a - b), 0.0f) / k;
            f32 blended = std::min(a, b) - h * h * k * 0.25f;
            // Bias toward A or B based on blendFactor
            result.data[i] = a * invBlend + b * blendFactor + blended * k;
            result.data[i] /= (1.0f + k);
        } else {
            // Hard blend
            result.data[i] = a * invBlend + b * blendFactor;
        }
    }

    return result;
}

} // namespace Renderer
} // namespace Enjin
