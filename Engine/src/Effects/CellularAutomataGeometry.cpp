#include "Enjin/Effects/CellularAutomataGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Enjin {
namespace Effects {

// ---------------------------------------------------------------------------
// Marching cubes lookup tables (Paul Bourke / public domain)
// ---------------------------------------------------------------------------

// Edge table: for each of the 256 cube configurations, which of the 12 edges
// are intersected (bit mask).
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

// Triangle table: for each of the 256 cube configurations, up to 5 triangles
// (15 edge indices), terminated by -1.
static const i32 s_MCTriTable[256][16] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  8,  3,  9,  8,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  3,  1,  2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 9,  2, 10,  0,  2,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 2,  8,  3,  2, 10,  8, 10,  9,  8, -1, -1, -1, -1, -1, -1, -1},
    { 3, 11,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0, 11,  2,  8, 11,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  9,  0,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1, 11,  2,  1,  9, 11,  9,  8, 11, -1, -1, -1, -1, -1, -1, -1},
    { 3, 10,  1, 11, 10,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0, 10,  1,  0,  8, 10,  8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    { 3,  9,  0,  3, 11,  9, 11, 10,  9, -1, -1, -1, -1, -1, -1, -1},
    { 9,  8, 10, 10,  8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  7,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  3,  0,  7,  3,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  1,  9,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  1,  9,  4,  7,  1,  7,  3,  1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 10,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 3,  4,  7,  3,  0,  4,  1,  2, 10, -1, -1, -1, -1, -1, -1, -1},
    { 9,  2, 10,  9,  0,  2,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1},
    { 2, 10,  9,  2,  9,  7,  2,  7,  3,  7,  9,  4, -1, -1, -1, -1},
    { 8,  4,  7,  3, 11,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11,  4,  7, 11,  2,  4,  2,  0,  4, -1, -1, -1, -1, -1, -1, -1},
    { 9,  0,  1,  8,  4,  7,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1},
    { 4,  7, 11,  9,  4, 11,  9, 11,  2,  9,  2,  1, -1, -1, -1, -1},
    { 3, 10,  1,  3, 11, 10,  7,  8,  4, -1, -1, -1, -1, -1, -1, -1},
    { 1, 11, 10,  1,  4, 11,  1,  0,  4,  7, 11,  4, -1, -1, -1, -1},
    { 4,  7,  8,  9,  0, 11,  9, 11, 10, 11,  0,  3, -1, -1, -1, -1},
    { 4,  7, 11,  4, 11,  9,  9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    { 9,  5,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 9,  5,  4,  0,  8,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  5,  4,  1,  5,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 8,  5,  4,  8,  3,  5,  3,  1,  5, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 10,  9,  5,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 3,  0,  8,  1,  2, 10,  4,  9,  5, -1, -1, -1, -1, -1, -1, -1},
    { 5,  2, 10,  5,  4,  2,  4,  0,  2, -1, -1, -1, -1, -1, -1, -1},
    { 2, 10,  5,  3,  2,  5,  3,  5,  4,  3,  4,  8, -1, -1, -1, -1},
    { 9,  5,  4,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0, 11,  2,  0,  8, 11,  4,  9,  5, -1, -1, -1, -1, -1, -1, -1},
    { 0,  5,  4,  0,  1,  5,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1},
    { 2,  1,  5,  2,  5,  8,  2,  8, 11,  4,  8,  5, -1, -1, -1, -1},
    {10,  3, 11, 10,  1,  3,  9,  5,  4, -1, -1, -1, -1, -1, -1, -1},
    { 4,  9,  5,  0,  8,  1,  8, 10,  1,  8, 11, 10, -1, -1, -1, -1},
    { 5,  4,  0,  5,  0, 11,  5, 11, 10, 11,  0,  3, -1, -1, -1, -1},
    { 5,  4,  8,  5,  8, 10, 10,  8, 11, -1, -1, -1, -1, -1, -1, -1},
    { 9,  7,  8,  5,  7,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 9,  3,  0,  9,  5,  3,  5,  7,  3, -1, -1, -1, -1, -1, -1, -1},
    { 0,  7,  8,  0,  1,  7,  1,  5,  7, -1, -1, -1, -1, -1, -1, -1},
    { 1,  5,  3,  3,  5,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 9,  7,  8,  9,  5,  7, 10,  1,  2, -1, -1, -1, -1, -1, -1, -1},
    {10,  1,  2,  9,  5,  0,  5,  3,  0,  5,  7,  3, -1, -1, -1, -1},
    { 8,  0,  2,  8,  2,  5,  8,  5,  7, 10,  5,  2, -1, -1, -1, -1},
    { 2, 10,  5,  2,  5,  3,  3,  5,  7, -1, -1, -1, -1, -1, -1, -1},
    { 7,  9,  5,  7,  8,  9,  3, 11,  2, -1, -1, -1, -1, -1, -1, -1},
    { 9,  5,  7,  9,  7,  2,  9,  2,  0,  2,  7, 11, -1, -1, -1, -1},
    { 2,  3, 11,  0,  1,  8,  1,  7,  8,  1,  5,  7, -1, -1, -1, -1},
    {11,  2,  1, 11,  1,  7,  7,  1,  5, -1, -1, -1, -1, -1, -1, -1},
    { 9,  5,  8,  8,  5,  7, 10,  1,  3, 10,  3, 11, -1, -1, -1, -1},
    { 5,  7,  0,  5,  0,  9,  7, 11,  0,  1,  0, 10, 11, 10,  0, -1},
    {11, 10,  0, 11,  0,  3, 10,  5,  0,  8,  0,  7,  5,  7,  0, -1},
    {11, 10,  5,  7, 11,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10,  6,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  3,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 9,  0,  1,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  8,  3,  1,  9,  8,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1},
    { 1,  6,  5,  2,  6,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  6,  5,  1,  2,  6,  3,  0,  8, -1, -1, -1, -1, -1, -1, -1},
    { 9,  6,  5,  9,  0,  6,  0,  2,  6, -1, -1, -1, -1, -1, -1, -1},
    { 5,  9,  8,  5,  8,  2,  5,  2,  6,  3,  2,  8, -1, -1, -1, -1},
    { 2,  3, 11, 10,  6,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11,  0,  8, 11,  2,  0, 10,  6,  5, -1, -1, -1, -1, -1, -1, -1},
    { 0,  1,  9,  2,  3, 11,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1},
    { 5, 10,  6,  1,  9,  2,  9, 11,  2,  9,  8, 11, -1, -1, -1, -1},
    { 6,  3, 11,  6,  5,  3,  5,  1,  3, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8, 11,  0, 11,  5,  0,  5,  1,  5, 11,  6, -1, -1, -1, -1},
    { 3, 11,  6,  0,  3,  6,  0,  6,  5,  0,  5,  9, -1, -1, -1, -1},
    { 6,  5,  9,  6,  9, 11, 11,  9,  8, -1, -1, -1, -1, -1, -1, -1},
    { 5, 10,  6,  4,  7,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  3,  0,  4,  7,  3,  6,  5, 10, -1, -1, -1, -1, -1, -1, -1},
    { 1,  9,  0,  5, 10,  6,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1},
    {10,  6,  5,  1,  9,  7,  1,  7,  3,  7,  9,  4, -1, -1, -1, -1},
    { 6,  1,  2,  6,  5,  1,  4,  7,  8, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2,  5,  5,  2,  6,  3,  0,  4,  3,  4,  7, -1, -1, -1, -1},
    { 8,  4,  7,  9,  0,  5,  0,  6,  5,  0,  2,  6, -1, -1, -1, -1},
    { 7,  3,  9,  7,  9,  4,  3,  2,  9,  5,  9,  6,  2,  6,  9, -1},
    { 3, 11,  2,  7,  8,  4, 10,  6,  5, -1, -1, -1, -1, -1, -1, -1},
    { 5, 10,  6,  4,  7,  2,  4,  2,  0,  2,  7, 11, -1, -1, -1, -1},
    { 0,  1,  9,  4,  7,  8,  2,  3, 11,  5, 10,  6, -1, -1, -1, -1},
    { 9,  2,  1,  9, 11,  2,  9,  4, 11,  7, 11,  4,  5, 10,  6, -1},
    { 8,  4,  7,  3, 11,  5,  3,  5,  1,  5, 11,  6, -1, -1, -1, -1},
    { 5,  1, 11,  5, 11,  6,  1,  0, 11,  7, 11,  4,  0,  4, 11, -1},
    { 0,  5,  9,  0,  6,  5,  0,  3,  6, 11,  6,  3,  8,  4,  7, -1},
    { 6,  5,  9,  6,  9, 11,  4,  7,  9,  7, 11,  9, -1, -1, -1, -1},
    {10,  4,  9,  6,  4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4, 10,  6,  4,  9, 10,  0,  8,  3, -1, -1, -1, -1, -1, -1, -1},
    {10,  0,  1, 10,  6,  0,  6,  4,  0, -1, -1, -1, -1, -1, -1, -1},
    { 8,  3,  1,  8,  1,  6,  8,  6,  4,  6,  1, 10, -1, -1, -1, -1},
    { 1,  4,  9,  1,  2,  4,  2,  6,  4, -1, -1, -1, -1, -1, -1, -1},
    { 3,  0,  8,  1,  2,  9,  2,  4,  9,  2,  6,  4, -1, -1, -1, -1},
    { 0,  2,  4,  4,  2,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 8,  3,  2,  8,  2,  4,  4,  2,  6, -1, -1, -1, -1, -1, -1, -1},
    {10,  4,  9, 10,  6,  4, 11,  2,  3, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  2,  2,  8, 11,  4,  9, 10,  4, 10,  6, -1, -1, -1, -1},
    { 3, 11,  2,  0,  1,  6,  0,  6,  4,  6,  1, 10, -1, -1, -1, -1},
    { 6,  4,  1,  6,  1, 10,  4,  8,  1,  2,  1, 11,  8, 11,  1, -1},
    { 9,  6,  4,  9,  3,  6,  9,  1,  3, 11,  6,  3, -1, -1, -1, -1},
    { 8, 11,  1,  8,  1,  0, 11,  6,  1,  9,  1,  4,  6,  4,  1, -1},
    { 3, 11,  6,  3,  6,  0,  0,  6,  4, -1, -1, -1, -1, -1, -1, -1},
    { 6,  4,  8, 11,  6,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 7, 10,  6,  7,  8, 10,  8,  9, 10, -1, -1, -1, -1, -1, -1, -1},
    { 0,  7,  3,  0, 10,  7,  0,  9, 10,  6,  7, 10, -1, -1, -1, -1},
    {10,  6,  7,  1, 10,  7,  1,  7,  8,  1,  8,  0, -1, -1, -1, -1},
    {10,  6,  7, 10,  7,  1,  1,  7,  3, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2,  6,  1,  6,  8,  1,  8,  9,  8,  6,  7, -1, -1, -1, -1},
    { 2,  6,  9,  2,  9,  1,  6,  7,  9,  0,  9,  3,  7,  3,  9, -1},
    { 7,  8,  0,  7,  0,  6,  6,  0,  2, -1, -1, -1, -1, -1, -1, -1},
    { 7,  3,  2,  6,  7,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 2,  3, 11, 10,  6,  8, 10,  8,  9,  8,  6,  7, -1, -1, -1, -1},
    { 2,  0,  7,  2,  7, 11,  0,  9,  7,  6,  7, 10,  9, 10,  7, -1},
    { 1,  8,  0,  1,  7,  8,  1, 10,  7,  6,  7, 10,  2,  3, 11, -1},
    {11,  2,  1, 11,  1,  7, 10,  6,  1,  6,  7,  1, -1, -1, -1, -1},
    { 8,  9,  6,  8,  6,  7,  9,  1,  6, 11,  6,  3,  1,  3,  6, -1},
    { 0,  9,  1, 11,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 7,  8,  0,  7,  0,  6,  3, 11,  0, 11,  6,  0, -1, -1, -1, -1},
    { 7, 11,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 7,  6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 3,  0,  8, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  1,  9, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 8,  1,  9,  8,  3,  1, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1},
    {10,  1,  2,  6, 11,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 10,  3,  0,  8,  6, 11,  7, -1, -1, -1, -1, -1, -1, -1},
    { 2,  9,  0,  2, 10,  9,  6, 11,  7, -1, -1, -1, -1, -1, -1, -1},
    { 6, 11,  7,  2, 10,  3, 10,  8,  3, 10,  9,  8, -1, -1, -1, -1},
    { 7,  2,  3,  6,  2,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 7,  0,  8,  7,  6,  0,  6,  2,  0, -1, -1, -1, -1, -1, -1, -1},
    { 2,  7,  6,  2,  3,  7,  0,  1,  9, -1, -1, -1, -1, -1, -1, -1},
    { 1,  6,  2,  1,  8,  6,  1,  9,  8,  8,  7,  6, -1, -1, -1, -1},
    {10,  7,  6, 10,  1,  7,  1,  3,  7, -1, -1, -1, -1, -1, -1, -1},
    {10,  7,  6,  1,  7, 10,  1,  8,  7,  1,  0,  8, -1, -1, -1, -1},
    { 0,  3,  7,  0,  7, 10,  0, 10,  9,  6, 10,  7, -1, -1, -1, -1},
    { 7,  6, 10,  7, 10,  8,  8, 10,  9, -1, -1, -1, -1, -1, -1, -1},
    { 6,  8,  4, 11,  8,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 3,  6, 11,  3,  0,  6,  0,  4,  6, -1, -1, -1, -1, -1, -1, -1},
    { 8,  6, 11,  8,  4,  6,  9,  0,  1, -1, -1, -1, -1, -1, -1, -1},
    { 9,  4,  6,  9,  6,  3,  9,  3,  1, 11,  3,  6, -1, -1, -1, -1},
    { 6,  8,  4,  6, 11,  8,  2, 10,  1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 10,  3,  0, 11,  0,  6, 11,  0,  4,  6, -1, -1, -1, -1},
    { 4, 11,  8,  4,  6, 11,  0,  2,  9,  2, 10,  9, -1, -1, -1, -1},
    {10,  9,  3, 10,  3,  2,  9,  4,  3, 11,  3,  6,  4,  6,  3, -1},
    { 8,  2,  3,  8,  4,  2,  4,  6,  2, -1, -1, -1, -1, -1, -1, -1},
    { 0,  4,  2,  4,  6,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  9,  0,  2,  3,  4,  2,  4,  6,  4,  3,  8, -1, -1, -1, -1},
    { 1,  9,  4,  1,  4,  2,  2,  4,  6, -1, -1, -1, -1, -1, -1, -1},
    { 8,  1,  3,  8,  6,  1,  8,  4,  6,  6, 10,  1, -1, -1, -1, -1},
    {10,  1,  0, 10,  0,  6,  6,  0,  4, -1, -1, -1, -1, -1, -1, -1},
    { 4,  6,  3,  4,  3,  8,  6, 10,  3,  0,  3,  9, 10,  9,  3, -1},
    {10,  9,  4,  6, 10,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  9,  5,  7,  6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  3,  4,  9,  5, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1},
    { 5,  0,  1,  5,  4,  0,  7,  6, 11, -1, -1, -1, -1, -1, -1, -1},
    {11,  7,  6,  8,  3,  4,  3,  5,  4,  3,  1,  5, -1, -1, -1, -1},
    { 9,  5,  4, 10,  1,  2,  7,  6, 11, -1, -1, -1, -1, -1, -1, -1},
    { 6, 11,  7,  1,  2, 10,  0,  8,  3,  4,  9,  5, -1, -1, -1, -1},
    { 7,  6, 11,  5,  4, 10,  4,  2, 10,  4,  0,  2, -1, -1, -1, -1},
    { 3,  4,  8,  3,  5,  4,  3,  2,  5, 10,  5,  2, 11,  7,  6, -1},
    { 7,  2,  3,  7,  6,  2,  5,  4,  9, -1, -1, -1, -1, -1, -1, -1},
    { 9,  5,  4,  0,  8,  6,  0,  6,  2,  6,  8,  7, -1, -1, -1, -1},
    { 3,  6,  2,  3,  7,  6,  1,  5,  0,  5,  4,  0, -1, -1, -1, -1},
    { 6,  2,  8,  6,  8,  7,  2,  1,  8,  4,  8,  5,  1,  5,  8, -1},
    { 9,  5,  4, 10,  1,  6,  1,  7,  6,  1,  3,  7, -1, -1, -1, -1},
    { 1,  6, 10,  1,  7,  6,  1,  0,  7,  8,  7,  0,  9,  5,  4, -1},
    { 4,  0, 10,  4, 10,  5,  0,  3, 10,  6, 10,  7,  3,  7, 10, -1},
    { 7,  6, 10,  7, 10,  8,  5,  4, 10,  4,  8, 10, -1, -1, -1, -1},
    { 6,  9,  5,  6, 11,  9, 11,  8,  9, -1, -1, -1, -1, -1, -1, -1},
    { 3,  6, 11,  0,  6,  3,  0,  5,  6,  0,  9,  5, -1, -1, -1, -1},
    { 0, 11,  8,  0,  5, 11,  0,  1,  5,  5,  6, 11, -1, -1, -1, -1},
    { 6, 11,  3,  6,  3,  5,  5,  3,  1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 10,  9,  5, 11,  9, 11,  8, 11,  5,  6, -1, -1, -1, -1},
    { 0, 11,  3,  0,  6, 11,  0,  9,  6,  5,  6,  9,  1,  2, 10, -1},
    {11,  8,  5, 11,  5,  6,  8,  0,  5, 10,  5,  2,  0,  2,  5, -1},
    { 6, 11,  3,  6,  3,  5,  2, 10,  3, 10,  5,  3, -1, -1, -1, -1},
    { 5,  8,  9,  5,  2,  8,  5,  6,  2,  3,  8,  2, -1, -1, -1, -1},
    { 9,  5,  6,  9,  6,  0,  0,  6,  2, -1, -1, -1, -1, -1, -1, -1},
    { 1,  5,  8,  1,  8,  0,  5,  6,  8,  3,  8,  2,  6,  2,  8, -1},
    { 1,  5,  6,  2,  1,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  3,  6,  1,  6, 10,  3,  8,  6,  5,  6,  9,  8,  9,  6, -1},
    {10,  1,  0, 10,  0,  6,  9,  5,  0,  5,  6,  0, -1, -1, -1, -1},
    { 0,  3,  8,  5,  6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10,  5,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11,  5, 10,  7,  5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11,  5, 10, 11,  7,  5,  8,  3,  0, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  7,  5, 10, 11,  1,  9,  0, -1, -1, -1, -1, -1, -1, -1},
    {10, 7,  5, 10, 11,  7,  9,  8,  1,  8,  3,  1, -1, -1, -1, -1},
    {11,  1,  2, 11,  7,  1,  7,  5,  1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  3,  1,  2,  7,  1,  7,  5,  7,  2, 11, -1, -1, -1, -1},
    { 9,  7,  5,  9,  2,  7,  9,  0,  2,  2, 11,  7, -1, -1, -1, -1},
    { 7,  5,  2,  7,  2, 11,  5,  9,  2,  3,  2,  8,  9,  8,  2, -1},
    { 2,  5, 10,  2,  3,  5,  3,  7,  5, -1, -1, -1, -1, -1, -1, -1},
    { 8,  2,  0,  8,  5,  2,  8,  7,  5, 10,  2,  5, -1, -1, -1, -1},
    { 9,  0,  1,  5, 10,  3,  5,  3,  7,  3, 10,  2, -1, -1, -1, -1},
    { 9,  8,  2,  9,  2,  1,  8,  7,  2, 10,  2,  5,  7,  5,  2, -1},
    { 1,  3,  5,  3,  7,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  7,  0,  7,  1,  1,  7,  5, -1, -1, -1, -1, -1, -1, -1},
    { 9,  0,  3,  9,  3,  5,  5,  3,  7, -1, -1, -1, -1, -1, -1, -1},
    { 9,  8,  7,  5,  9,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5,  8,  4,  5, 10,  8, 10, 11,  8, -1, -1, -1, -1, -1, -1, -1},
    { 5,  0,  4,  5, 11,  0,  5, 10, 11, 11,  3,  0, -1, -1, -1, -1},
    { 0,  1,  9,  8,  4, 10,  8, 10, 11, 10,  4,  5, -1, -1, -1, -1},
    {10, 11,  4, 10,  4,  5, 11,  3,  4,  9,  4,  1,  3,  1,  4, -1},
    { 2,  5,  1,  2,  8,  5,  2, 11,  8,  4,  5,  8, -1, -1, -1, -1},
    { 0,  4, 11,  0, 11,  3,  4,  5, 11,  2, 11,  1,  5,  1, 11, -1},
    { 0,  2,  5,  0,  5,  9,  2, 11,  5,  4,  5,  8, 11,  8,  5, -1},
    { 9,  4,  5,  2, 11,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 2,  5, 10,  3,  5,  2,  3,  4,  5,  3,  8,  4, -1, -1, -1, -1},
    { 5, 10,  2,  5,  2,  4,  4,  2,  0, -1, -1, -1, -1, -1, -1, -1},
    { 3, 10,  2,  3,  5, 10,  3,  8,  5,  4,  5,  8,  0,  1,  9, -1},
    { 5, 10,  2,  5,  2,  4,  1,  9,  2,  9,  4,  2, -1, -1, -1, -1},
    { 8,  4,  5,  8,  5,  3,  3,  5,  1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  4,  5,  1,  0,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 8,  4,  5,  8,  5,  3,  9,  0,  5,  0,  3,  5, -1, -1, -1, -1},
    { 9,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4, 11,  7,  4,  9, 11,  9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    { 0,  8,  3,  4,  9,  7,  9, 11,  7,  9, 10, 11, -1, -1, -1, -1},
    { 1, 10, 11,  1, 11,  4,  1,  4,  0,  7,  4, 11, -1, -1, -1, -1},
    { 3,  1,  4,  3,  4,  8,  1, 10,  4,  7,  4, 11, 10, 11,  4, -1},
    { 4, 11,  7,  9, 11,  4,  9,  2, 11,  9,  1,  2, -1, -1, -1, -1},
    { 9,  7,  4,  9, 11,  7,  9,  1, 11,  2, 11,  1,  0,  8,  3, -1},
    {11,  7,  4, 11,  4,  2,  2,  4,  0, -1, -1, -1, -1, -1, -1, -1},
    {11,  7,  4, 11,  4,  2,  8,  3,  4,  3,  2,  4, -1, -1, -1, -1},
    { 2,  9, 10,  2,  7,  9,  2,  3,  7,  7,  4,  9, -1, -1, -1, -1},
    { 9, 10,  7,  9,  7,  4, 10,  2,  7,  8,  7,  0,  2,  0,  7, -1},
    { 3,  7, 10,  3, 10,  2,  7,  4, 10,  1, 10,  0,  4,  0, 10, -1},
    { 1, 10,  2,  8,  7,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  9,  1,  4,  1,  7,  7,  1,  3, -1, -1, -1, -1, -1, -1, -1},
    { 4,  9,  1,  4,  1,  7,  0,  8,  1,  8,  7,  1, -1, -1, -1, -1},
    { 4,  0,  3,  7,  4,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 4,  8,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 9, 10,  8, 10, 11,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 3,  0,  9,  3,  9, 11, 11,  9, 10, -1, -1, -1, -1, -1, -1, -1},
    { 0,  1, 10,  0, 10,  8,  8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    { 3,  1, 10, 11,  3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  2, 11,  1, 11,  9,  9, 11,  8, -1, -1, -1, -1, -1, -1, -1},
    { 3,  0,  9,  3,  9, 11,  1,  2,  9,  2, 11,  9, -1, -1, -1, -1},
    { 0,  2, 11,  8,  0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 3,  2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 2,  3,  8,  2,  8, 10, 10,  8,  9, -1, -1, -1, -1, -1, -1, -1},
    { 9, 10,  2,  0,  9,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 2,  3,  8,  2,  8, 10,  0,  1,  8,  1, 10,  8, -1, -1, -1, -1},
    { 1, 10,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  3,  8,  9,  1,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  9,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 0,  3,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

// Cube corner offsets (x, y, z) for the 8 vertices of a marching cube
static const i32 s_MCCornerOffsets[8][3] = {
    {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
    {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
};

// Edge endpoints (which two corners each of the 12 edges connects)
static const i32 s_MCEdgeCorners[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

// ---------------------------------------------------------------------------
// xorshift32 PRNG (matches project convention)
// ---------------------------------------------------------------------------
static u32 Xorshift32(u32& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// ---------------------------------------------------------------------------
// CellularAutomataGeometry implementation
// ---------------------------------------------------------------------------

void CellularAutomataGeometry::Initialize(const CAGeoConfig& config) {
    m_Config = config;

    // Clamp dimensions to reasonable range
    m_Config.width = std::max(m_Config.width, 1u);
    m_Config.height = std::max(m_Config.height, 1u);
    m_Config.depth = std::max(m_Config.depth, 1u);

    // Apply preset rule bits if not Custom
    if (m_Config.rule != CARule::Custom) {
        GetRuleBits(m_Config.rule, m_Config.birthRule, m_Config.survivalRule, m_Config.states);
    }

    usize totalCells = static_cast<usize>(m_Config.width) * m_Config.height * m_Config.depth;
    m_Grid.assign(totalCells, 0);
    m_NextGrid.assign(totalCells, 0);
    m_Generation = 0;
    m_Timer = 0.0f;

    // Random fill
    u32 rngState = m_Config.seed;
    if (rngState == 0) rngState = 0xDEADBEEF;

    f32 threshold = m_Config.initialFillPercent / 100.0f;

    // For Rule110, only initialize the first row
    if (m_Config.rule == CARule::Rule110) {
        for (u32 x = 0; x < m_Config.width; ++x) {
            f32 r = static_cast<f32>(Xorshift32(rngState) & 0xFFFF) / 65536.0f;
            m_Grid[CellIndex(x, 0, 0)] = (r < threshold) ? 1 : 0;
        }
        // Run all rows as time steps
        for (u32 row = 1; row < m_Config.height; ++row) {
            for (u32 x = 0; x < m_Config.width; ++x) {
                i32 left  = static_cast<i32>(x) - 1;
                i32 right = static_cast<i32>(x) + 1;

                u8 l = 0, c = 0, r2 = 0;
                c = m_Grid[CellIndex(x, row - 1, 0)];

                if (m_Config.wrapEdges) {
                    l = m_Grid[CellIndex(WrapCoord(left, m_Config.width), row - 1, 0)];
                    r2 = m_Grid[CellIndex(WrapCoord(right, m_Config.width), row - 1, 0)];
                } else {
                    l = (left >= 0) ? m_Grid[CellIndex(static_cast<u32>(left), row - 1, 0)] : 0;
                    r2 = (right < static_cast<i32>(m_Config.width)) ? m_Grid[CellIndex(static_cast<u32>(right), row - 1, 0)] : 0;
                }

                u8 pattern = (l << 2) | (c << 1) | r2;
                // Rule 110 binary: 01101110
                u8 rule110 = 0x6E;
                m_Grid[CellIndex(x, row, 0)] = (rule110 >> pattern) & 1;
            }
        }
    } else {
        for (usize i = 0; i < totalCells; ++i) {
            f32 r = static_cast<f32>(Xorshift32(rngState) & 0xFFFF) / 65536.0f;
            m_Grid[i] = (r < threshold) ? 1 : 0;
        }
    }
}

void CellularAutomataGeometry::Update(f32 dt) {
    m_Timer += dt;
    if (m_Config.updateInterval > 0.0f && m_Timer >= m_Config.updateInterval) {
        m_Timer -= m_Config.updateInterval;
        Step();
    }
}

void CellularAutomataGeometry::Step() {
    bool is3D = (m_Config.depth > 1);

    if (m_Config.rule == CARule::Rule110) {
        // For Rule110: shift all rows up, compute new bottom row
        usize totalCells = static_cast<usize>(m_Config.width) * m_Config.height * m_Config.depth;
        std::memcpy(m_NextGrid.data(), m_Grid.data(), totalCells);

        // Shift rows up by 1
        for (u32 y = 0; y + 1 < m_Config.height; ++y) {
            for (u32 x = 0; x < m_Config.width; ++x) {
                m_NextGrid[CellIndex(x, y, 0)] = m_Grid[CellIndex(x, y + 1, 0)];
            }
        }

        // Compute new bottom row from old bottom row
        u32 lastRow = m_Config.height - 1;
        for (u32 x = 0; x < m_Config.width; ++x) {
            i32 left  = static_cast<i32>(x) - 1;
            i32 right = static_cast<i32>(x) + 1;

            u8 l = 0, c = 0, r = 0;
            c = m_Grid[CellIndex(x, lastRow, 0)];

            if (m_Config.wrapEdges) {
                l = m_Grid[CellIndex(WrapCoord(left, m_Config.width), lastRow, 0)];
                r = m_Grid[CellIndex(WrapCoord(right, m_Config.width), lastRow, 0)];
            } else {
                l = (left >= 0) ? m_Grid[CellIndex(static_cast<u32>(left), lastRow, 0)] : 0;
                r = (right < static_cast<i32>(m_Config.width)) ? m_Grid[CellIndex(static_cast<u32>(right), lastRow, 0)] : 0;
            }

            u8 pattern = (l << 2) | (c << 1) | r;
            u8 rule110 = 0x6E;
            m_NextGrid[CellIndex(x, lastRow, 0)] = (rule110 >> pattern) & 1;
        }

        std::swap(m_Grid, m_NextGrid);
        ++m_Generation;
        return;
    }

    if (m_Config.rule == CARule::BriansBrain) {
        // 3-state: Alive(1) -> Dying(2) -> Dead(0) -> Alive(1) if exactly 2 alive neighbors
        for (u32 z = 0; z < m_Config.depth; ++z) {
            for (u32 y = 0; y < m_Config.height; ++y) {
                for (u32 x = 0; x < m_Config.width; ++x) {
                    u8 current = m_Grid[CellIndex(x, y, z)];
                    if (current == 1) {
                        // Alive -> Dying
                        m_NextGrid[CellIndex(x, y, z)] = 2;
                    } else if (current == 2) {
                        // Dying -> Dead
                        m_NextGrid[CellIndex(x, y, z)] = 0;
                    } else {
                        // Dead -> check neighbors (count only alive=1 cells)
                        u32 aliveNeighbors = 0;
                        if (is3D) {
                            // Count 26 neighbors, only state==1
                            for (i32 dz = -1; dz <= 1; ++dz) {
                                for (i32 dy = -1; dy <= 1; ++dy) {
                                    for (i32 dx = -1; dx <= 1; ++dx) {
                                        if (dx == 0 && dy == 0 && dz == 0) continue;
                                        i32 nx = static_cast<i32>(x) + dx;
                                        i32 ny = static_cast<i32>(y) + dy;
                                        i32 nz = static_cast<i32>(z) + dz;
                                        if (m_Config.wrapEdges) {
                                            nx = static_cast<i32>(WrapCoord(nx, m_Config.width));
                                            ny = static_cast<i32>(WrapCoord(ny, m_Config.height));
                                            nz = static_cast<i32>(WrapCoord(nz, m_Config.depth));
                                        } else {
                                            if (nx < 0 || nx >= static_cast<i32>(m_Config.width)) continue;
                                            if (ny < 0 || ny >= static_cast<i32>(m_Config.height)) continue;
                                            if (nz < 0 || nz >= static_cast<i32>(m_Config.depth)) continue;
                                        }
                                        if (m_Grid[CellIndex(static_cast<u32>(nx), static_cast<u32>(ny), static_cast<u32>(nz))] == 1)
                                            ++aliveNeighbors;
                                    }
                                }
                            }
                        } else {
                            // Count 8 neighbors (2D), only state==1
                            for (i32 dy = -1; dy <= 1; ++dy) {
                                for (i32 dx = -1; dx <= 1; ++dx) {
                                    if (dx == 0 && dy == 0) continue;
                                    i32 nx = static_cast<i32>(x) + dx;
                                    i32 ny = static_cast<i32>(y) + dy;
                                    if (m_Config.wrapEdges) {
                                        nx = static_cast<i32>(WrapCoord(nx, m_Config.width));
                                        ny = static_cast<i32>(WrapCoord(ny, m_Config.height));
                                    } else {
                                        if (nx < 0 || nx >= static_cast<i32>(m_Config.width)) continue;
                                        if (ny < 0 || ny >= static_cast<i32>(m_Config.height)) continue;
                                    }
                                    if (m_Grid[CellIndex(static_cast<u32>(nx), static_cast<u32>(ny), 0)] == 1)
                                        ++aliveNeighbors;
                                }
                            }
                        }
                        m_NextGrid[CellIndex(x, y, z)] = (aliveNeighbors == 2) ? 1 : 0;
                    }
                }
            }
        }
    } else {
        // Standard birth/survival bitmask rules (GameOfLife, HighLife, DayAndNight, Seeds, Diamoeba, Custom)
        for (u32 z = 0; z < m_Config.depth; ++z) {
            for (u32 y = 0; y < m_Config.height; ++y) {
                for (u32 x = 0; x < m_Config.width; ++x) {
                    u32 neighbors = is3D ? CountNeighbors3D(x, y, z) : CountNeighbors2D(x, y);
                    u8 current = m_Grid[CellIndex(x, y, z)];
                    u32 neighborBit = (neighbors < 32) ? (1u << neighbors) : 0u;

                    if (current == 0) {
                        // Dead cell: born if neighbor count is in birth rule
                        m_NextGrid[CellIndex(x, y, z)] = (m_Config.birthRule & neighborBit) ? 1 : 0;
                    } else {
                        // Live cell: survive if neighbor count is in survival rule
                        m_NextGrid[CellIndex(x, y, z)] = (m_Config.survivalRule & neighborBit) ? 1 : 0;
                    }
                }
            }
        }
    }

    std::swap(m_Grid, m_NextGrid);
    ++m_Generation;
}

void CellularAutomataGeometry::Reset() {
    Initialize(m_Config);
}

void CellularAutomataGeometry::SetCell(u32 x, u32 y, u32 z, u8 state) {
    if (x >= m_Config.width || y >= m_Config.height || z >= m_Config.depth) return;
    m_Grid[CellIndex(x, y, z)] = state;
}

u8 CellularAutomataGeometry::GetCell(u32 x, u32 y, u32 z) const {
    if (x >= m_Config.width || y >= m_Config.height || z >= m_Config.depth) return 0;
    return m_Grid[CellIndex(x, y, z)];
}

void CellularAutomataGeometry::StampGlider(u32 x, u32 y) {
    // Classic 5-cell glider pattern:
    //  .X.
    //  ..X
    //  XXX
    static const i32 pattern[][2] = {
        {1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}
    };
    for (const auto& p : pattern) {
        u32 px = x + static_cast<u32>(p[0]);
        u32 py = y + static_cast<u32>(p[1]);
        if (px < m_Config.width && py < m_Config.height) {
            SetCell(px, py, 0, 1);
        }
    }
}

void CellularAutomataGeometry::StampPulsar(u32 x, u32 y) {
    // Period-3 oscillator (pulsar) relative offsets from top-left of 15x15 bounding box
    // Centered at (x+7, y+7)
    static const i32 pattern[][2] = {
        // Top arm
        {2,0}, {3,0}, {4,0}, {8,0}, {9,0}, {10,0},
        // Second row from top
        {0,2}, {5,2}, {7,2}, {12,2},
        {0,3}, {5,3}, {7,3}, {12,3},
        {0,4}, {5,4}, {7,4}, {12,4},
        // Upper-middle
        {2,5}, {3,5}, {4,5}, {8,5}, {9,5}, {10,5},
        // Lower-middle
        {2,7}, {3,7}, {4,7}, {8,7}, {9,7}, {10,7},
        // Below middle
        {0,8}, {5,8}, {7,8}, {12,8},
        {0,9}, {5,9}, {7,9}, {12,9},
        {0,10}, {5,10}, {7,10}, {12,10},
        // Bottom arm
        {2,12}, {3,12}, {4,12}, {8,12}, {9,12}, {10,12}
    };
    for (const auto& p : pattern) {
        u32 px = x + static_cast<u32>(p[0]);
        u32 py = y + static_cast<u32>(p[1]);
        if (px < m_Config.width && py < m_Config.height) {
            SetCell(px, py, 0, 1);
        }
    }
}

void CellularAutomataGeometry::StampGosperGun(u32 x, u32 y) {
    // Gosper glider gun (36 live cells in a 36x9 bounding box)
    static const i32 pattern[][2] = {
        // Left square
        {0,4}, {0,5}, {1,4}, {1,5},
        // Left ship
        {10,4}, {10,5}, {10,6},
        {11,3}, {11,7},
        {12,2}, {12,8},
        {13,2}, {13,8},
        {14,5},
        {15,3}, {15,7},
        {16,4}, {16,5}, {16,6},
        {17,5},
        // Right ship
        {20,2}, {20,3}, {20,4},
        {21,2}, {21,3}, {21,4},
        {22,1}, {22,5},
        {24,0}, {24,1}, {24,5}, {24,6},
        // Right square
        {34,2}, {34,3}, {35,2}, {35,3}
    };
    for (const auto& p : pattern) {
        u32 px = x + static_cast<u32>(p[0]);
        u32 py = y + static_cast<u32>(p[1]);
        if (px < m_Config.width && py < m_Config.height) {
            SetCell(px, py, 0, 1);
        }
    }
}

CAMeshData CellularAutomataGeometry::GenerateMesh() const {
    CAMeshData mesh;
    mesh.generation = m_Generation;
    mesh.liveCellCount = GetLiveCellCount();

    switch (m_Config.meshMode) {
        case CAMeshMode::Voxels:        GenerateVoxelMesh(mesh);         break;
        case CAMeshMode::MarchingCubes: GenerateMarchingCubesMesh(mesh); break;
        case CAMeshMode::PointCloud:    GeneratePointCloudMesh(mesh);    break;
    }

    return mesh;
}

u32 CellularAutomataGeometry::GetLiveCellCount() const {
    u32 count = 0;
    for (u8 cell : m_Grid) {
        if (cell == 1) ++count;
    }
    return count;
}

const char* CellularAutomataGeometry::GetRuleName(CARule rule) {
    switch (rule) {
        case CARule::GameOfLife:   return "Game of Life (B3/S23)";
        case CARule::HighLife:     return "HighLife (B36/S23)";
        case CARule::DayAndNight:  return "Day & Night (B3678/S34678)";
        case CARule::Seeds:        return "Seeds (B2/S)";
        case CARule::BriansBrain:  return "Brian's Brain (3-state)";
        case CARule::Rule110:      return "Rule 110 (1D elementary)";
        case CARule::Diamoeba:     return "Diamoeba (B35678/S5678)";
        case CARule::Custom:       return "Custom";
    }
    return "Unknown";
}

void CellularAutomataGeometry::GetRuleBits(CARule rule, u32& birth, u32& survival, u32& states) {
    states = 2;
    switch (rule) {
        case CARule::GameOfLife:
            birth    = (1u << 3);                                       // B3
            survival = (1u << 2) | (1u << 3);                          // S23
            break;
        case CARule::HighLife:
            birth    = (1u << 3) | (1u << 6);                          // B36
            survival = (1u << 2) | (1u << 3);                          // S23
            break;
        case CARule::DayAndNight:
            birth    = (1u << 3) | (1u << 6) | (1u << 7) | (1u << 8); // B3678
            survival = (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 8); // S34678
            break;
        case CARule::Seeds:
            birth    = (1u << 2);                                       // B2
            survival = 0;                                                // S(none)
            break;
        case CARule::BriansBrain:
            birth    = (1u << 2);  // Born with exactly 2 alive neighbors
            survival = 0;          // Not used (3-state logic handles transitions)
            states   = 3;
            break;
        case CARule::Rule110:
            birth    = 0;  // Not used (elementary 1D rule)
            survival = 0;
            break;
        case CARule::Diamoeba:
            birth    = (1u << 3) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8); // B35678
            survival = (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8);             // S5678
            break;
        case CARule::Custom:
            // Keep existing values
            break;
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

u32 CellularAutomataGeometry::CellIndex(u32 x, u32 y, u32 z) const {
    return x + y * m_Config.width + z * m_Config.width * m_Config.height;
}

u32 CellularAutomataGeometry::WrapCoord(i32 v, u32 max) const {
    if (max == 0) return 0;
    i32 m = static_cast<i32>(max);
    return static_cast<u32>(((v % m) + m) % m);
}

u32 CellularAutomataGeometry::CountNeighbors2D(u32 x, u32 y) const {
    u32 count = 0;
    for (i32 dy = -1; dy <= 1; ++dy) {
        for (i32 dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            i32 nx = static_cast<i32>(x) + dx;
            i32 ny = static_cast<i32>(y) + dy;

            if (m_Config.wrapEdges) {
                nx = static_cast<i32>(WrapCoord(nx, m_Config.width));
                ny = static_cast<i32>(WrapCoord(ny, m_Config.height));
            } else {
                if (nx < 0 || nx >= static_cast<i32>(m_Config.width)) continue;
                if (ny < 0 || ny >= static_cast<i32>(m_Config.height)) continue;
            }

            if (m_Grid[CellIndex(static_cast<u32>(nx), static_cast<u32>(ny), 0)] > 0) {
                ++count;
            }
        }
    }
    return count;
}

u32 CellularAutomataGeometry::CountNeighbors3D(u32 x, u32 y, u32 z) const {
    u32 count = 0;
    for (i32 dz = -1; dz <= 1; ++dz) {
        for (i32 dy = -1; dy <= 1; ++dy) {
            for (i32 dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                i32 nx = static_cast<i32>(x) + dx;
                i32 ny = static_cast<i32>(y) + dy;
                i32 nz = static_cast<i32>(z) + dz;

                if (m_Config.wrapEdges) {
                    nx = static_cast<i32>(WrapCoord(nx, m_Config.width));
                    ny = static_cast<i32>(WrapCoord(ny, m_Config.height));
                    nz = static_cast<i32>(WrapCoord(nz, m_Config.depth));
                } else {
                    if (nx < 0 || nx >= static_cast<i32>(m_Config.width)) continue;
                    if (ny < 0 || ny >= static_cast<i32>(m_Config.height)) continue;
                    if (nz < 0 || nz >= static_cast<i32>(m_Config.depth)) continue;
                }

                if (m_Grid[CellIndex(static_cast<u32>(nx), static_cast<u32>(ny), static_cast<u32>(nz))] > 0) {
                    ++count;
                }
            }
        }
    }
    return count;
}

bool CellularAutomataGeometry::IsFaceVisible(u32 x, u32 y, u32 z, i32 dx, i32 dy, i32 dz) const {
    i32 nx = static_cast<i32>(x) + dx;
    i32 ny = static_cast<i32>(y) + dy;
    i32 nz = static_cast<i32>(z) + dz;

    // Face at grid boundary is always visible (no wrapping for face culling)
    if (nx < 0 || nx >= static_cast<i32>(m_Config.width)) return true;
    if (ny < 0 || ny >= static_cast<i32>(m_Config.height)) return true;
    if (nz < 0 || nz >= static_cast<i32>(m_Config.depth)) return true;

    // Visible if neighbor is empty (state == 0)
    return m_Grid[CellIndex(static_cast<u32>(nx), static_cast<u32>(ny), static_cast<u32>(nz))] == 0;
}

// ---------------------------------------------------------------------------
// Voxel mesh generation (greedy face culling)
// ---------------------------------------------------------------------------

void CellularAutomataGeometry::GenerateVoxelMesh(CAMeshData& mesh) const {
    f32 cs = m_Config.cellSize;

    // 6 face directions: +X, -X, +Y, -Y, +Z, -Z
    static const i32 faceDir[6][3] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };

    // Face normal vectors
    static const Math::Vector3 faceNormals[6] = {
        { 1.0f,  0.0f,  0.0f}, {-1.0f,  0.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f},
        { 0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f}
    };

    // Quad vertex offsets for each face (4 corners, CCW winding when viewed from outside)
    // Each face has 4 vertices defined as offsets from cell origin (0,0,0) to (1,1,1)
    static const f32 faceVerts[6][4][3] = {
        // +X face (x=1)
        {{1,0,0}, {1,1,0}, {1,1,1}, {1,0,1}},
        // -X face (x=0)
        {{0,0,1}, {0,1,1}, {0,1,0}, {0,0,0}},
        // +Y face (y=1)
        {{0,1,0}, {0,1,1}, {1,1,1}, {1,1,0}},
        // -Y face (y=0)
        {{0,0,1}, {0,0,0}, {1,0,0}, {1,0,1}},
        // +Z face (z=1)
        {{0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}},
        // -Z face (z=0)
        {{1,0,0}, {0,0,0}, {0,1,0}, {1,1,0}}
    };

    // Reserve approximate space (6 faces * 4 verts * ~30% fill)
    usize estimatedCells = static_cast<usize>(m_Config.width) * m_Config.height * m_Config.depth;
    usize estimateLive = static_cast<usize>(estimatedCells * m_Config.initialFillPercent / 100.0f);
    mesh.vertices.reserve(estimateLive * 6); // rough estimate
    mesh.indices.reserve(estimateLive * 9);

    for (u32 z = 0; z < m_Config.depth; ++z) {
        for (u32 y = 0; y < m_Config.height; ++y) {
            for (u32 x = 0; x < m_Config.width; ++x) {
                u8 cellState = m_Grid[CellIndex(x, y, z)];
                if (cellState == 0) continue;

                // Determine color based on state
                Math::Vector3 color = m_Config.liveColor;
                if (cellState == 2 && m_Config.states >= 3) {
                    color = m_Config.dyingColor;
                }

                f32 ox = static_cast<f32>(x) * cs;
                f32 oy = static_cast<f32>(y) * cs;
                f32 oz = static_cast<f32>(z) * cs;

                // For each of 6 faces, only emit if neighbor in that direction is empty
                for (u32 f = 0; f < 6; ++f) {
                    if (!IsFaceVisible(x, y, z, faceDir[f][0], faceDir[f][1], faceDir[f][2])) continue;

                    u32 baseIdx = static_cast<u32>(mesh.vertices.size());

                    // 4 vertices for this face
                    for (u32 v = 0; v < 4; ++v) {
                        CAVertex vert;
                        vert.position.x = ox + faceVerts[f][v][0] * cs;
                        vert.position.y = oy + faceVerts[f][v][1] * cs;
                        vert.position.z = oz + faceVerts[f][v][2] * cs;
                        vert.normal = faceNormals[f];
                        vert.color = color;
                        mesh.vertices.push_back(vert);
                    }

                    // 2 triangles (CCW: 0-1-2, 0-2-3)
                    mesh.indices.push_back(baseIdx);
                    mesh.indices.push_back(baseIdx + 1);
                    mesh.indices.push_back(baseIdx + 2);
                    mesh.indices.push_back(baseIdx);
                    mesh.indices.push_back(baseIdx + 2);
                    mesh.indices.push_back(baseIdx + 3);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Marching cubes mesh generation
// ---------------------------------------------------------------------------

void CellularAutomataGeometry::GenerateMarchingCubesMesh(CAMeshData& mesh) const {
    f32 cs = m_Config.cellSize;
    f32 iso = m_Config.isoLevel;

    // For 2D grids (depth==1), we treat it as a thin 3D slab with depth=2 so marching
    // cubes can produce geometry. We sample the same 2D data for z=0 and z=1.
    u32 w = m_Config.width;
    u32 h = m_Config.height;
    u32 d = m_Config.depth;
    bool flat = (d == 1);
    u32 dSample = flat ? 2 : d;

    // Lambda to sample density at grid point (returns 0.0 or 1.0)
    auto SampleDensity = [&](u32 gx, u32 gy, u32 gz) -> f32 {
        if (gx >= w || gy >= h) return 0.0f;
        u32 cz = flat ? 0 : std::min(gz, d - 1);
        return (m_Grid[CellIndex(gx, gy, cz)] >= 1) ? 1.0f : 0.0f;
    };

    // Iterate over cubes: (w-1) x (h-1) x (dSample-1) cubes
    u32 cubesX = (w > 1) ? w - 1 : 1;
    u32 cubesY = (h > 1) ? h - 1 : 1;
    u32 cubesZ = (dSample > 1) ? dSample - 1 : 1;

    for (u32 gz = 0; gz < cubesZ; ++gz) {
        for (u32 gy = 0; gy < cubesY; ++gy) {
            for (u32 gx = 0; gx < cubesX; ++gx) {
                // Sample 8 corners of this cube
                f32 cornerValues[8];
                for (u32 c = 0; c < 8; ++c) {
                    u32 cx = gx + static_cast<u32>(s_MCCornerOffsets[c][0]);
                    u32 cy = gy + static_cast<u32>(s_MCCornerOffsets[c][1]);
                    u32 cz = gz + static_cast<u32>(s_MCCornerOffsets[c][2]);
                    cornerValues[c] = SampleDensity(cx, cy, cz);
                }

                // Compute cube index from corner inside/outside classification
                u32 cubeIndex = 0;
                for (u32 c = 0; c < 8; ++c) {
                    if (cornerValues[c] >= iso) {
                        cubeIndex |= (1u << c);
                    }
                }

                // Skip fully inside or fully outside
                if (s_MCEdgeTable[cubeIndex] == 0) continue;

                // Compute interpolated vertex positions on intersected edges
                Math::Vector3 edgeVerts[12];

                for (u32 e = 0; e < 12; ++e) {
                    if (!(s_MCEdgeTable[cubeIndex] & (1u << e))) continue;

                    i32 c0 = s_MCEdgeCorners[e][0];
                    i32 c1 = s_MCEdgeCorners[e][1];

                    f32 v0 = cornerValues[c0];
                    f32 v1 = cornerValues[c1];

                    // Interpolation factor
                    f32 t = 0.5f;
                    f32 diff = v1 - v0;
                    if (std::abs(diff) > 1e-6f) {
                        t = (iso - v0) / diff;
                    }
                    t = std::max(0.0f, std::min(1.0f, t));

                    // Corner world positions
                    f32 x0 = (static_cast<f32>(gx) + s_MCCornerOffsets[c0][0]) * cs;
                    f32 y0 = (static_cast<f32>(gy) + s_MCCornerOffsets[c0][1]) * cs;
                    f32 z0 = (static_cast<f32>(gz) + s_MCCornerOffsets[c0][2]) * cs;
                    f32 x1 = (static_cast<f32>(gx) + s_MCCornerOffsets[c1][0]) * cs;
                    f32 y1 = (static_cast<f32>(gy) + s_MCCornerOffsets[c1][1]) * cs;
                    f32 z1 = (static_cast<f32>(gz) + s_MCCornerOffsets[c1][2]) * cs;

                    edgeVerts[e].x = x0 + t * (x1 - x0);
                    edgeVerts[e].y = y0 + t * (y1 - y0);
                    edgeVerts[e].z = z0 + t * (z1 - z0);
                }

                // Emit triangles from the triangle table
                const i32* triList = s_MCTriTable[cubeIndex];
                for (u32 t = 0; triList[t] != -1; t += 3) {
                    Math::Vector3 p0 = edgeVerts[triList[t]];
                    Math::Vector3 p1 = edgeVerts[triList[t + 1]];
                    Math::Vector3 p2 = edgeVerts[triList[t + 2]];

                    // Compute face normal via cross product
                    Math::Vector3 e1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
                    Math::Vector3 e2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);
                    Math::Vector3 normal(
                        e1.y * e2.z - e1.z * e2.y,
                        e1.z * e2.x - e1.x * e2.z,
                        e1.x * e2.y - e1.y * e2.x
                    );
                    f32 nLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                    if (nLen > 1e-8f) {
                        normal.x /= nLen;
                        normal.y /= nLen;
                        normal.z /= nLen;
                    }

                    u32 baseIdx = static_cast<u32>(mesh.vertices.size());

                    CAVertex v0Vert; v0Vert.position = p0; v0Vert.normal = normal; v0Vert.color = m_Config.liveColor;
                    CAVertex v1Vert; v1Vert.position = p1; v1Vert.normal = normal; v1Vert.color = m_Config.liveColor;
                    CAVertex v2Vert; v2Vert.position = p2; v2Vert.normal = normal; v2Vert.color = m_Config.liveColor;

                    mesh.vertices.push_back(v0Vert);
                    mesh.vertices.push_back(v1Vert);
                    mesh.vertices.push_back(v2Vert);

                    mesh.indices.push_back(baseIdx);
                    mesh.indices.push_back(baseIdx + 1);
                    mesh.indices.push_back(baseIdx + 2);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Point cloud mesh generation (low-poly sphere per live cell)
// ---------------------------------------------------------------------------

void CellularAutomataGeometry::GeneratePointCloudMesh(CAMeshData& mesh) const {
    f32 cs = m_Config.cellSize;
    f32 radius = cs * 0.4f;

    // Generate an octahedron (8 triangles, 6 vertices) per live cell as a lightweight sphere proxy
    static const Math::Vector3 octaVerts[6] = {
        { 0.0f,  1.0f,  0.0f},  // top
        { 0.0f, -1.0f,  0.0f},  // bottom
        { 1.0f,  0.0f,  0.0f},  // +X
        {-1.0f,  0.0f,  0.0f},  // -X
        { 0.0f,  0.0f,  1.0f},  // +Z
        { 0.0f,  0.0f, -1.0f}   // -Z
    };

    // 8 triangles (CCW from outside)
    static const u32 octaIndices[24] = {
        0, 2, 4,  // top +X +Z
        0, 4, 3,  // top +Z -X
        0, 3, 5,  // top -X -Z
        0, 5, 2,  // top -Z +X
        1, 4, 2,  // bottom +Z +X
        1, 3, 4,  // bottom -X +Z
        1, 5, 3,  // bottom -Z -X
        1, 2, 5   // bottom +X -Z
    };

    usize estimatedLive = static_cast<usize>(
        static_cast<f32>(m_Config.width) * m_Config.height * m_Config.depth * m_Config.initialFillPercent / 100.0f);
    mesh.vertices.reserve(estimatedLive * 6);
    mesh.indices.reserve(estimatedLive * 24);

    for (u32 z = 0; z < m_Config.depth; ++z) {
        for (u32 y = 0; y < m_Config.height; ++y) {
            for (u32 x = 0; x < m_Config.width; ++x) {
                u8 cellState = m_Grid[CellIndex(x, y, z)];
                if (cellState == 0) continue;

                Math::Vector3 color = m_Config.liveColor;
                if (cellState == 2 && m_Config.states >= 3) {
                    color = m_Config.dyingColor;
                }

                // Cell center
                f32 cx = (static_cast<f32>(x) + 0.5f) * cs;
                f32 cy = (static_cast<f32>(y) + 0.5f) * cs;
                f32 cz = (static_cast<f32>(z) + 0.5f) * cs;

                u32 baseIdx = static_cast<u32>(mesh.vertices.size());

                // Add 6 octahedron vertices
                for (u32 v = 0; v < 6; ++v) {
                    CAVertex vert;
                    vert.position.x = cx + octaVerts[v].x * radius;
                    vert.position.y = cy + octaVerts[v].y * radius;
                    vert.position.z = cz + octaVerts[v].z * radius;
                    vert.normal = octaVerts[v]; // Octahedron normals = vertex directions
                    vert.color = color;
                    mesh.vertices.push_back(vert);
                }

                // Add 24 indices (8 triangles)
                for (u32 i = 0; i < 24; ++i) {
                    mesh.indices.push_back(baseIdx + octaIndices[i]);
                }
            }
        }
    }
}

} // namespace Effects
} // namespace Enjin
