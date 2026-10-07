import AVFoundation
import NaturalLanguage

/// Lê as respostas em voz alta com a voz do próprio relógio, no idioma do texto.
@MainActor
final class Speaker: NSObject, AVSpeechSynthesizerDelegate {
    static let shared = Speaker()

    private let synth = AVSpeechSynthesizer()
    private(set) var isSpeaking = false
    var onChange: ((Bool) -> Void)?

    override init() {
        super.init()
        synth.delegate = self
    }

    func speak(_ text: String) {
        let clean = Self.speakable(text)
        guard !clean.isEmpty else { return }
        stop()
        try? AVAudioSession.sharedInstance().setCategory(.playback, mode: .spokenAudio, options: [.duckOthers])
        try? AVAudioSession.sharedInstance().setActive(true)
        let utterance = AVSpeechUtterance(string: clean)
        utterance.voice = AVSpeechSynthesisVoice(language: Self.language(of: clean))
        utterance.rate = AVSpeechUtteranceDefaultSpeechRate
        synth.speak(utterance)
        setSpeaking(true)
    }

    func stop() {
        if synth.isSpeaking { synth.stopSpeaking(at: .immediate) }
        setSpeaking(false)
    }

    private func setSpeaking(_ value: Bool) {
        isSpeaking = value
        onChange?(value)
        if !value { try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation) }
    }

    nonisolated func speechSynthesizer(_ synthesizer: AVSpeechSynthesizer, didFinish utterance: AVSpeechUtterance) {
        Task { @MainActor in self.setSpeaking(false) }
    }

    nonisolated func speechSynthesizer(_ synthesizer: AVSpeechSynthesizer, didCancel utterance: AVSpeechUtterance) {
        Task { @MainActor in self.setSpeaking(false) }
    }

    /// Tira o que não se lê bem: blocos de código, markdown, URLs longas.
    nonisolated static func speakable(_ text: String) -> String {
        var s = text
        s = s.replacingOccurrences(of: "```[\\s\\S]*?```", with: " ", options: .regularExpression)
        s = s.replacingOccurrences(of: "`([^`]*)`", with: "$1", options: .regularExpression)
        s = s.replacingOccurrences(of: "\\[([^\\]]+)\\]\\([^)]+\\)", with: "$1", options: .regularExpression)
        s = s.replacingOccurrences(of: "https?://\\S+", with: "", options: .regularExpression)
        s = s.replacingOccurrences(of: "(?m)^\\s*[#>*-]+\\s*", with: "", options: .regularExpression)
        s = s.replacingOccurrences(of: "[*_~|]", with: "", options: .regularExpression)
        s = s.replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
        return s.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    nonisolated static func language(of text: String) -> String {
        let recognizer = NLLanguageRecognizer()
        recognizer.processString(text)
        guard let lang = recognizer.dominantLanguage else { return AVSpeechSynthesisVoice.currentLanguageCode() }
        let current = AVSpeechSynthesisVoice.currentLanguageCode()
        // Mantém a variante regional do relógio quando o idioma bate (pt-BR, en-GB…).
        if current.hasPrefix(lang.rawValue) { return current }
        switch lang {
        case .portuguese: return "pt-BR"
        case .english: return "en-US"
        case .spanish: return "es-ES"
        default: return lang.rawValue
        }
    }
}
