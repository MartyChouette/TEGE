// The shared-memory editor bridge, on whatever platform this is built for.
//
// Connect returned false on anything but Windows with a "not implemented" log,
// while the factory still handed back a transport object, so a caller on Linux
// or macOS held something that looked usable and silently never carried a
// message. Everything between Connect and Disconnect — the length-prefixed
// ring, Send, Receive, HasPendingMessages — was already portable; only the
// mapping was not.
//
// These tests use two transports in one process over the same endpoint, which
// is the same arrangement as an editor and a runtime on one machine.
#include "EnjinTest.h"
#include "Enjin/Editor/EditorProtocol.h"

#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

// A distinct endpoint per test: the segment outlives the process that made it
// on POSIX, so reusing one name would let a crashed earlier run seed state.
std::string Endpoint(const char* suffix) {
    return std::string("enjin_test_") + suffix;
}

EditorMessage Msg(const char* text, u64 seq) {
    return EditorMessage::MakeString(EditorMessageType::LogMessage, text, seq);
}

} // namespace

ENJIN_TEST(SharedMemoryTransport, ConnectSucceedsOnThisPlatform) {
    // Arrange
    auto transport = CreateSharedMemoryTransport();
    ENJIN_ASSERT_TRUE(transport != nullptr);

    // Act
    const bool connected = transport->Connect(Endpoint("connect"));

    // Assert
    ENJIN_EXPECT_TRUE(connected);
    ENJIN_EXPECT_TRUE(transport->IsConnected());
    transport->Disconnect();
    ENJIN_EXPECT_TRUE(!transport->IsConnected());
}

ENJIN_TEST(SharedMemoryTransport, MessageCrossesFromOneEndToTheOther) {
    // Arrange: two transports on one endpoint, as an editor and a runtime would.
    auto writer = CreateSharedMemoryTransport();
    auto reader = CreateSharedMemoryTransport();
    ENJIN_ASSERT_TRUE(writer->Connect(Endpoint("roundtrip")));
    ENJIN_ASSERT_TRUE(reader->Connect(Endpoint("roundtrip")));
    ENJIN_EXPECT_TRUE(!reader->HasPendingMessages());

    // Act
    ENJIN_ASSERT_TRUE(writer->Send(Msg("hello from the editor", 7)));

    // Assert: the payload and the header both survive the crossing.
    ENJIN_ASSERT_TRUE(reader->HasPendingMessages());
    EditorMessage got;
    ENJIN_ASSERT_TRUE(reader->Receive(got));
    ENJIN_EXPECT_TRUE(got.PayloadAsString() == "hello from the editor");
    ENJIN_EXPECT_TRUE(got.header.sequenceId == 7);
    ENJIN_EXPECT_TRUE(got.header.type == static_cast<u16>(EditorMessageType::LogMessage));
    ENJIN_EXPECT_TRUE(!reader->HasPendingMessages());

    reader->Disconnect();
    writer->Disconnect();
}

ENJIN_TEST(SharedMemoryTransport, MessagesArriveInOrder) {
    // Arrange
    auto writer = CreateSharedMemoryTransport();
    auto reader = CreateSharedMemoryTransport();
    ENJIN_ASSERT_TRUE(writer->Connect(Endpoint("ordering")));
    ENJIN_ASSERT_TRUE(reader->Connect(Endpoint("ordering")));

    // Act
    ENJIN_ASSERT_TRUE(writer->Send(Msg("first", 1)));
    ENJIN_ASSERT_TRUE(writer->Send(Msg("second", 2)));
    ENJIN_ASSERT_TRUE(writer->Send(Msg("third", 3)));

    // Assert
    for (const char* expected : {"first", "second", "third"}) {
        EditorMessage got;
        ENJIN_ASSERT_TRUE(reader->Receive(got));
        ENJIN_EXPECT_TRUE(got.PayloadAsString() == expected);
    }
    ENJIN_EXPECT_TRUE(!reader->HasPendingMessages());

    reader->Disconnect();
    writer->Disconnect();
}

ENJIN_TEST(SharedMemoryTransport, AnEmptyRingReadsNothingRatherThanGarbage) {
    // Arrange
    auto transport = CreateSharedMemoryTransport();
    ENJIN_ASSERT_TRUE(transport->Connect(Endpoint("empty")));

    // Act
    EditorMessage got;
    const bool received = transport->Receive(got);

    // Assert
    ENJIN_EXPECT_TRUE(!received);
    ENJIN_EXPECT_TRUE(!transport->HasPendingMessages());
    transport->Disconnect();
}

ENJIN_TEST(SharedMemoryTransport, SendingBeforeConnectingIsRefused) {
    // Arrange: the transport exists but was never connected.
    auto transport = CreateSharedMemoryTransport();
    ENJIN_ASSERT_TRUE(transport != nullptr);

    // Act
    const bool sent = transport->Send(Msg("nobody is listening", 1));

    // Assert
    ENJIN_EXPECT_TRUE(!sent);
    ENJIN_EXPECT_TRUE(!transport->IsConnected());
}

ENJIN_TEST(SharedMemoryTransport, EndpointsWithPathCharactersStayOneSegment) {
    // Arrange: POSIX shared-memory names may hold one leading slash and no
    // others, so an endpoint carrying slashes or dots must not become a path.
    auto transport = CreateSharedMemoryTransport();

    // Act
    const bool connected = transport->Connect("../../etc/enjin test.name");

    // Assert: it connects, which means the name was sanitized rather than
    // rejected or resolved somewhere outside the shared-memory namespace.
    ENJIN_EXPECT_TRUE(connected);
    if (connected) {
        ENJIN_EXPECT_TRUE(transport->Send(Msg("still works", 1)));
        transport->Disconnect();
    }
}

ENJIN_TEST_MAIN()
