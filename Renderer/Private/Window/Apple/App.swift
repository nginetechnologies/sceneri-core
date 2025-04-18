#if !os(iOS) && !os(macOS)
import SwiftUI
import CompositorServices

struct MetalLayerConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities,
                           configuration: inout LayerRenderer.Configuration)
    {
        //let supportsFoveation = capabilities.supportsFoveation
        //let supportedLayouts = capabilities.supportedLayouts(options: supportsFoveation ? [.foveationEnabled] : [])
        configuration.layout = .shared
        configuration.isFoveationEnabled = false//supportsFoveation
        configuration.colorFormat = .rgba16Float
        configuration.colorUsage = [.shaderRead, .shaderWrite, .renderTarget ]
        configuration.depthUsage = [ .shaderRead, .renderTarget ]
    }
}

struct ViewControllerRepresentable: UIViewControllerRepresentable {

    func makeUIViewController(context: Context) -> ViewController {
        return ViewController()
    }

    func updateUIViewController(_ uiViewController: ViewController, context: Context) {
    }
}

struct MetalViewRepresentable: UIViewRepresentable {

    func makeUIView(context: Context) -> MetalView {
        return MetalView()
    }

    func updateUIView(_ uiView: MetalView, context: Context) {
    }
}

struct ContentView: View {
    @State private var isPresented = true

    var body: some View {
        ViewControllerRepresentable()
        MetalViewRepresentable()
    }
}

class AppState: ObservableObject {
    static let shared = AppState()

    var pLogicalDevice : UnsafeMutableRawPointer? = nil;

    @Environment(\.openImmersiveSpace) public var openImmersiveSpace
    @Environment(\.dismissImmersiveSpace) public var dismissImmersiveSpace
}

@objc
class MixedRealityInterface : NSObject {
    @objc public func startFullImmersiveSpace(pLogicalDevice : UnsafeMutableRawPointer) async -> Void{
        AppState.shared.pLogicalDevice = pLogicalDevice;
        await AppState.shared.openImmersiveSpace(id: "FullyImmersiveSpace")
    }
    @objc public func startMixedImmersiveSpace(pLogicalDevice : UnsafeMutableRawPointer) async -> Void {
        AppState.shared.pLogicalDevice = pLogicalDevice;
        await AppState.shared.openImmersiveSpace(id: "MixedImmersiveSpace")
    }
    @objc public func stopImmersiveSpace() async -> Void {
        await AppState.shared.dismissImmersiveSpace()
    }
}

@main
struct MetalApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject var appState = AppState.shared

    var body: some Scene {
        WindowGroup {
            ContentView()
        }

        ImmersiveSpace(id: "FullImmersiveSpace") {
            CompositorLayer(configuration: MetalLayerConfiguration()) { layerRenderer in
                appDelegate.initializeLayerRenderer(layerRenderer, pLogicalDevice: AppState.shared.pLogicalDevice);
            }
        }.immersionStyle(selection: .constant(.full), in: .full)

        ImmersiveSpace(id: "MixedImmersiveSpace") {
            CompositorLayer(configuration: MetalLayerConfiguration()) { layerRenderer in
                appDelegate.initializeLayerRenderer(layerRenderer, pLogicalDevice: AppState.shared.pLogicalDevice);
            }
        }.immersionStyle(selection: .constant(.mixed), in: .mixed)
    }
}
#endif
