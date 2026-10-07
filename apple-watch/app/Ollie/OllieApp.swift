import SwiftUI
import UserNotifications
import WatchKit

@main
struct OllieApp: App {
    @WKApplicationDelegateAdaptor private var delegate: AppDelegate
    @Environment(\.scenePhase) private var phase
    @State private var store = AppStore.shared

    var body: some Scene {
        WindowGroup {
            RootView()
                .environment(store)
                .tint(.clawd)
        }
        .onChange(of: phase) { _, phase in
            if phase == .active, store.isPaired {
                store.startPolling()
            } else if phase != .active {
                store.stopPolling()
            }
        }
    }
}

final class AppDelegate: NSObject, WKApplicationDelegate, UNUserNotificationCenterDelegate {
    func applicationDidFinishLaunching() {
        let center = UNUserNotificationCenter.current()
        center.delegate = self
        let allow = UNNotificationAction(identifier: "ALLOW", title: String(localized: "Allow"), options: [])
        let deny = UNNotificationAction(identifier: "DENY", title: String(localized: "Deny"), options: [.destructive])
        let listen = UNNotificationAction(identifier: "LISTEN", title: String(localized: "Listen"), options: [.foreground])
        center.setNotificationCategories([
            UNNotificationCategory(identifier: "PERMISSION", actions: [allow, deny], intentIdentifiers: []),
            UNNotificationCategory(identifier: "SESSION_DONE", actions: [listen], intentIdentifiers: []),
        ])
        #if DEBUG
        // Capturas de tela no simulador: `-OllieSkipPushPrompt YES` pula o pedido de permissão.
        if UserDefaults.standard.bool(forKey: "OllieSkipPushPrompt") { return }
        #endif
        center.requestAuthorization(options: [.alert, .sound, .badge]) { granted, _ in
            guard granted else { return }
            DispatchQueue.main.async { WKApplication.shared().registerForRemoteNotifications() }
        }
    }

    func didRegisterForRemoteNotifications(withDeviceToken deviceToken: Data) {
        Task { @MainActor in await AppStore.shared.setPushToken(deviceToken) }
    }

    func didFailToRegisterForRemoteNotificationsWithError(_ error: Error) {
        print("push registration failed:", error)
    }

    // Com o app aberto: mostra o aviso e, se for a resposta a um prompt do relógio, lê em voz alta.
    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        willPresent notification: UNNotification,
        withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void
    ) {
        let info = notification.request.content.userInfo
        let sessionId = info["sessionId"] as? String
        let reply = info["reply"] as? String
        Task { @MainActor in
            if let sessionId, let reply { AppStore.shared.handlePushReply(sessionId: sessionId, reply: reply) }
            await AppStore.shared.refresh()
        }
        completionHandler([.banner, .sound, .list])
    }

    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        didReceive response: UNNotificationResponse,
        withCompletionHandler completionHandler: @escaping () -> Void
    ) {
        let content = response.notification.request.content
        let action = response.actionIdentifier
        let permissionId = content.userInfo["permissionId"] as? String
        let text = (content.userInfo["reply"] as? String) ?? content.body
        nonisolated(unsafe) let done = completionHandler
        Task { @MainActor in
            switch action {
            case "ALLOW", "DENY":
                if let permissionId { await AppStore.shared.decide(permissionId: permissionId, allow: action == "ALLOW") }
            case "LISTEN":
                AppStore.shared.speak(text)
            default:
                break
            }
            done()
        }
    }
}
