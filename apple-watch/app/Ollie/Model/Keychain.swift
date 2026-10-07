import Foundation
import Security

/// Guarda o token do pareamento no Keychain do relógio, num grupo que o app e as complicações compartilham.
enum Keychain {
    // Grupo compartilhado pelo app e pelas complicações ("<Team ID>.<prefixo dos bundles>.shared") e serviço
    // (o prefixo dos bundles): do Info.plist, montado a partir de Config/*.xcconfig
    private static let accessGroup = Bundle.main.object(forInfoDictionaryKey: "OllieKeychainGroup") as? String ?? ""
    private static let service = Bundle.main.object(forInfoDictionaryKey: "OllieKeychainService") as? String ?? ""
    private static let account = "relay-token"

    static func load() -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecAttrAccessGroup as String: accessGroup,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne,
        ]
        var item: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess,
              let data = item as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    static func save(_ token: String) {
        delete()
        let attrs: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecAttrAccessGroup as String: accessGroup,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlock,
            kSecValueData as String: Data(token.utf8),
        ]
        SecItemAdd(attrs as CFDictionary, nil)
    }

    static func delete() {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecAttrAccessGroup as String: accessGroup,
        ]
        SecItemDelete(query as CFDictionary)
    }
}
