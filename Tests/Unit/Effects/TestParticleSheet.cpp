// The sprite sheet frame a CPU particle shows over its life.
//
// Particle lifetime counts DOWN to zero, and the desktop renderer read it as
// the age, so every sheet played backwards from its last frame.
#include "EnjinTest.h"
#include "Enjin/Effects/ParticleSheet.h"

using namespace Enjin;
using namespace Enjin::Effects;

ENJIN_TEST(ParticleSheet, test_sheet_newborn_particle_shows_the_first_frame) {
    // Arrange: a 4x2 sheet, a particle just spawned (all of its life remaining)
    const f32 maxLife = 2.0f;

    // Act
    const ParticleSheetFrame f = ComputeParticleSheetFrame(maxLife, maxLife, 4, 2);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.x, 0.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.y, 0.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvScale.x, 0.25f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvScale.y, 0.5f, 1e-6f);
}

ENJIN_TEST(ParticleSheet, test_sheet_dying_particle_shows_the_last_frame) {
    // Arrange: almost none of its life remaining
    const f32 maxLife = 2.0f;

    // Act
    const ParticleSheetFrame f = ComputeParticleSheetFrame(0.001f, maxLife, 4, 2);

    // Assert: frame 7 of 8 is column 3, row 1
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.x, 0.75f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.y, 0.5f, 1e-6f);
}

ENJIN_TEST(ParticleSheet, test_sheet_halfway_shows_the_middle_frame) {
    // Arrange / Act: half of a 2x2 sheet's life gone is frame 2 of 4
    const ParticleSheetFrame f = ComputeParticleSheetFrame(1.0f, 2.0f, 2, 2);

    // Assert: column 0, row 1
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.x, 0.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.y, 0.5f, 1e-6f);
}

ENJIN_TEST(ParticleSheet, test_sheet_single_cell_uses_the_whole_texture) {
    // Arrange / Act
    const ParticleSheetFrame f = ComputeParticleSheetFrame(0.5f, 2.0f, 1, 1);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.x, 0.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvOffset.y, 0.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvScale.x, 1.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvScale.y, 1.0f, 1e-6f);
}

ENJIN_TEST(ParticleSheet, test_sheet_counts_clamp_to_the_inspector_range) {
    // Arrange / Act: 40 columns is clamped to 16; 0 rows to 1
    const ParticleSheetFrame f = ComputeParticleSheetFrame(2.0f, 2.0f, 40, 0);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(f.uvScale.x, 1.0f / 16.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(f.uvScale.y, 1.0f, 1e-6f);
}

ENJIN_TEST_MAIN()
