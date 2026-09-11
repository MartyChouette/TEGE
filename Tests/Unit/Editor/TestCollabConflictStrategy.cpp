// The Conflict Resolution setting was decoration.
//
// Two combo boxes wrote m_ConflictStrategy -- one in the Collaboration panel, one
// in the collab UI window -- and NOTHING read it. DetectConflict had no caller
// anywhere. m_UnresolvedConflicts was never appended to, so the Conflicts list was
// permanently empty in three places and ResolveConflict was unreachable. Every
// session ran last-writer-wins no matter which of the four options was selected,
// and the only comment on the subject said so out loud:
//
//     // CRDT merge - the document decides if the remote op changes local state.
//     // No manual conflict resolution needed; LWW registers auto-converge.
//
// ResolveConflict had a second hole of its own: Merge matched neither arm of its
// if, so the "Merge" button did precisely what "Keep Mine" did while saying
// something else.
//
// The decision is now a pure function, which is the part that has to be right and
// the part a running editor cannot show you: three of the four strategies differed
// from the default only in what the combo box said.
#include "EnjinTest.h"
#include "Enjin/Editor/CollaborativeEditing.h"

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

EditOperation MakeOp(EditOpType type, u8 author, u64 clock) {
    EditOperation op;
    op.type = type;
    op.entityId = 42;
    op.authorId = author;
    op.lamportClock = clock;
    return op;
}

EditOperation MakeTransform(u8 author, u64 clock, const Math::Vector3& pos) {
    EditOperation op = MakeOp(EditOpType::ModifyTransform, author, clock);
    op.position = pos;
    return op;
}

constexpr u8 kSomeClient = 3;   // any peer that is not the host

} // namespace

// ---------------------------------------------------------------------------
// Each strategy has to do something DIFFERENT
// ---------------------------------------------------------------------------

ENJIN_TEST(CollabConflictStrategy, TheFourStrategiesDoNotAllBehaveTheSame) {
    // Arrange: one local edit and one conflicting remote edit, seen from a client.
    const EditOperation local = MakeOp(EditOpType::SetComponent, 1, 5);
    const EditOperation remote = MakeOp(EditOpType::SetComponent, kSomeClient, 6);

    // Act
    const ConflictOutcome lww =
        DecideConflict(ConflictStrategy::LastWriterWins, false, local, remote);
    const ConflictOutcome merge =
        DecideConflict(ConflictStrategy::Merge, false, local, remote);
    const ConflictOutcome reject =
        DecideConflict(ConflictStrategy::Reject, false, local, remote);

    // Assert: this is the regression. All three used to reach the same code.
    ENJIN_EXPECT_TRUE(lww == ConflictOutcome::ApplyRemote);
    ENJIN_EXPECT_TRUE(merge == ConflictOutcome::ApplyMerged);
    ENJIN_EXPECT_TRUE(reject == ConflictOutcome::HoldForReview);
    ENJIN_EXPECT_TRUE(lww != merge && merge != reject && lww != reject);
}

ENJIN_TEST(CollabConflictStrategy, RejectHoldsTheEditInsteadOfApplyingIt) {
    // "Ask (Manual)" is the only strategy allowed to stall, and the only reason the
    // Conflicts list exists. Applying the edit anyway would make the list a log of
    // things that already happened.
    const EditOperation local = MakeOp(EditOpType::RenameEntity, 1, 5);
    const EditOperation remote = MakeOp(EditOpType::RenameEntity, kSomeClient, 99);

    const ConflictOutcome outcome =
        DecideConflict(ConflictStrategy::Reject, false, local, remote);

    ENJIN_EXPECT_TRUE(outcome == ConflictOutcome::HoldForReview);
    // Even a much later remote edit is held. A clock comparison here would make the
    // setting last-writer-wins with an extra step.
    ENJIN_EXPECT_TRUE(outcome != ConflictOutcome::ApplyRemote);
}

// ---------------------------------------------------------------------------
// Host authority
// ---------------------------------------------------------------------------

ENJIN_TEST(CollabConflictStrategy, HostKeepsItsOwnEditAndResendsIt) {
    // The host discarding the remote edit is only half the job: the CRDT document
    // has already merged it, and the peer applied its own edit when it made it. The
    // local edit has to go back out with a fresh clock or the two scenes stay
    // different and both sides think they are in sync.
    const EditOperation local = MakeOp(EditOpType::SetComponent, kHostPeerId, 1);
    const EditOperation remote = MakeOp(EditOpType::SetComponent, kSomeClient, 500);

    const ConflictOutcome outcome =
        DecideConflict(ConflictStrategy::HostAuthority, /*isHost*/ true, local, remote);

    ENJIN_EXPECT_TRUE(outcome == ConflictOutcome::KeepLocalAndReassert);
}

ENJIN_TEST(CollabConflictStrategy, AClientDefersToTheHostWhateverTheClockSays) {
    // The host is always peer 0, so a client CAN tell an arbitrating edit from an
    // ordinary peer's. Respecting the clock here would make Host Authority into
    // last-writer-wins under another name.
    const EditOperation local = MakeOp(EditOpType::SetComponent, kSomeClient, 900);
    const EditOperation fromHost = MakeOp(EditOpType::SetComponent, kHostPeerId, 1);

    const ConflictOutcome outcome =
        DecideConflict(ConflictStrategy::HostAuthority, /*isHost*/ false, local, fromHost);

    ENJIN_EXPECT_TRUE(outcome == ConflictOutcome::ForceApplyRemote);
}

ENJIN_TEST(CollabConflictStrategy, TwoClientsConflictingLeaveItToTheClock) {
    // Neither is authoritative and the host is not involved, so there is nothing to
    // arbitrate. Holding or rejecting here would stall an edit no one can settle.
    const EditOperation local = MakeOp(EditOpType::SetComponent, 2, 10);
    const EditOperation remote = MakeOp(EditOpType::SetComponent, 3, 11);

    const ConflictOutcome outcome =
        DecideConflict(ConflictStrategy::HostAuthority, /*isHost*/ false, local, remote);

    ENJIN_EXPECT_TRUE(outcome == ConflictOutcome::ApplyRemote);
}

// ---------------------------------------------------------------------------
// The merge itself
// ---------------------------------------------------------------------------

ENJIN_TEST(CollabConflictStrategy, MergingTwoTransformsAveragesThePositions) {
    // Arrange
    const EditOperation local = MakeTransform(1, 10, Math::Vector3(0.0f, 0.0f, 0.0f));
    const EditOperation remote = MakeTransform(2, 11, Math::Vector3(10.0f, 4.0f, -6.0f));

    // Act
    const EditOperation merged = MergeOperations(local, remote);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(merged.position.x, 5.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(merged.position.y, 2.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(merged.position.z, -3.0f, 0.0001f);
    // And it is a real third value, not one of the two inputs.
    ENJIN_EXPECT_TRUE(merged.position.x != local.position.x);
    ENJIN_EXPECT_TRUE(merged.position.x != remote.position.x);
}

ENJIN_TEST(CollabConflictStrategy, MergeTakesRotationAndScaleFromTheLaterEdit) {
    // Rotation and scale have no meaningful average -- halfway between two scales is
    // a size neither person asked for -- so the later edit takes them whole.
    EditOperation local = MakeTransform(1, 50, Math::Vector3(0.0f, 0.0f, 0.0f));
    local.rotation = Math::Vector3(0.0f, 90.0f, 0.0f);
    local.scale = Math::Vector3(3.0f, 3.0f, 3.0f);

    EditOperation remote = MakeTransform(2, 10, Math::Vector3(2.0f, 0.0f, 0.0f));
    remote.rotation = Math::Vector3(0.0f, 0.0f, 0.0f);
    remote.scale = Math::Vector3(1.0f, 1.0f, 1.0f);

    // local has the higher clock, so its rotation and scale win.
    const EditOperation merged = MergeOperations(local, remote);
    ENJIN_EXPECT_FLOAT_NEAR(merged.rotation.y, 90.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(merged.scale.x, 3.0f, 0.0001f);
    // The position is still blended.
    ENJIN_EXPECT_FLOAT_NEAR(merged.position.x, 1.0f, 0.0001f);
}

ENJIN_TEST(CollabConflictStrategy, MergingNonTransformsTakesTheLaterEditWhole) {
    // Two JSON payloads for the same component do not average into a third valid
    // one. Last-writer-wins is the honest answer for everything but a transform,
    // and the tooltip says so rather than implying a blend happened.
    EditOperation local = MakeOp(EditOpType::SetComponent, 1, 10);
    local.componentKey = "light";
    local.dataJson = "{\"intensity\":1}";

    EditOperation remote = MakeOp(EditOpType::SetComponent, 2, 11);
    remote.componentKey = "light";
    remote.dataJson = "{\"intensity\":9}";

    const EditOperation merged = MergeOperations(local, remote);
    ENJIN_EXPECT_TRUE(merged.dataJson == remote.dataJson);

    // And the other direction, so the result follows the clock rather than always
    // preferring the remote.
    EditOperation olderRemote = MakeOp(EditOpType::SetComponent, 2, 3);
    olderRemote.componentKey = "light";
    olderRemote.dataJson = "{\"intensity\":9}";
    const EditOperation other = MergeOperations(local, olderRemote);
    ENJIN_EXPECT_TRUE(other.dataJson == local.dataJson);
}

ENJIN_TEST_MAIN()
