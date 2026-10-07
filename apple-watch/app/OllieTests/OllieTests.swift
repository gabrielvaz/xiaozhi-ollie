import Testing
import Foundation
@testable import Ollie

struct DecodingTests {
    @Test func decodesRelayState() throws {
        let json = """
        {"now":1791331258045,"sessions":[{"id":"s1","host":"mac","project":"api","title":"Run the tests","state":"working",
        "activity":{"tool":"Edit","target":"App.swift"},"turnStartedAt":1791331250000,"lastDurationSec":null,"lastMessage":null,
        "lastPrompt":"Run the tests","waitingMessage":null,"channel":true,"startedAt":1791331240000,"updatedAt":1791331255000},
        {"id":"s2","host":"mac","project":"web","title":"x","state":"something-new","activity":null,"turnStartedAt":null,
        "lastDurationSec":12,"lastMessage":"ok","lastPrompt":null,"waitingMessage":null,"channel":false,"startedAt":1,"updatedAt":2}],
        "permissions":[{"id":"p1","sessionId":"s1","sessionTitle":"Run the tests","tool":"Bash","description":"npm test","preview":null,"createdAt":1}],
        "usage":{"fiveHour":{"pct":23.5,"resetsAt":1738425600},"updatedAt":1791331250000},
        "hosts":[{"id":"h1","name":"mac","pairedAt":1,"lastSeenAt":2}]}
        """
        let state = try JSONDecoder().decode(RelayState.self, from: Data(json.utf8))
        #expect(state.sessions.count == 2)
        #expect(state.sessions[0].activity == Activity(tool: "Edit", target: "App.swift"))
        #expect(state.sessions[1].state == .idle) // estado desconhecido não quebra o app
        #expect(state.working == 1)
        #expect(state.permissions(for: "s1").count == 1)
        #expect(state.usage?.fiveHour?.resetDate == Date(timeIntervalSince1970: 1738425600))
        #expect(state.usage?.sevenDay == nil)
    }
}

struct FormatTests {
    @Test func elapsed() {
        #expect(Format.elapsed(42) == "0:42")
        #expect(Format.elapsed(725) == "12:05")
        #expect(Format.elapsed(3800) == "1:03:20")
        #expect(Format.elapsed(-5) == "0:00")
    }

    @Test func activityWithoutTarget() {
        #expect(Format.activity(Activity(tool: "CustomTool", target: nil)) == "CustomTool")
        #expect(Format.activity(Activity(tool: "CustomTool", target: "x")) == "CustomTool x")
        #expect(Format.activity(Activity(tool: "Bash", target: "Run the test suite")) == "Run the test suite")
    }

    @Test func speakableStripsMarkdown() {
        let text = "**Done.** All `42` tests pass.\n```swift\nlet x = 1\n```\nSee [the PR](https://github.com/a/b/pull/1)."
        #expect(Speaker.speakable(text) == "Done. All 42 tests pass. See the PR.")
    }
}
