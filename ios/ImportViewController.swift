import UIKit
import UniformTypeIdentifiers

/// Imports a user-selected 3DS image into the app sandbox and starts Zakuro.
final class ImportViewController: UIViewController, UIDocumentPickerDelegate {
    private let rightsSwitch = UISwitch()
    private let statusLabel = UILabel()
    private let runtimeButton = UIButton(type: .system)
    private let controlsButton = UIButton(type: .system)
    private let cheatsButton = UIButton(type: .system)
    private var controlsSaveTimestampAtDisplay: Double = 0

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = "Yo-kai Watch 2"
        controlsSaveTimestampAtDisplay = UserDefaults.standard.double(forKey: "ykw2.controls.lastSavedAt")

        let titleLabel = UILabel()
        titleLabel.text = "Importar una copia autorizada"
        titleLabel.font = .preferredFont(forTextStyle: .title2)
        titleLabel.textAlignment = .center

        let rightsLabel = UILabel()
        rightsLabel.text = "Tengo derecho a usar esta copia en este dispositivo"
        rightsLabel.numberOfLines = 0

        let rightsRow = UIStackView(arrangedSubviews: [rightsLabel, rightsSwitch])
        rightsRow.axis = .horizontal
        rightsRow.alignment = .center
        rightsRow.spacing = 12

        let button = UIButton(type: .system)
        button.setTitle("Seleccionar ROM", for: .normal)
        button.addTarget(self, action: #selector(selectRom), for: .touchUpInside)

        runtimeButton.setTitle("Iniciar Yo-kai Watch 2", for: .normal)
        runtimeButton.addTarget(self, action: #selector(openRuntime), for: .touchUpInside)
        runtimeButton.isHidden = true

        controlsButton.setTitle("Configurar controles táctiles", for: .normal)
        controlsButton.addTarget(self, action: #selector(openControlSettings), for: .touchUpInside)

        cheatsButton.addTarget(self, action: #selector(configureCheats), for: .touchUpInside)
        refreshCheatsButtonTitle()

        statusLabel.text = "Selecciona una copia local autorizada para comenzar."
        statusLabel.numberOfLines = 0
        statusLabel.textAlignment = .center

        let stack = UIStackView(arrangedSubviews: [titleLabel, rightsRow, button, runtimeButton, controlsButton, cheatsButton, statusLabel])
        stack.axis = .vertical
        stack.spacing = 20
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24),
            stack.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
        ])

        if let romURL = try? romURL(), FileManager.default.fileExists(atPath: romURL.path) {
            runtimeButton.isHidden = false
            statusLabel.text = "ROM guardada en este iPhone. Zakuro la abrirá desde el almacenamiento privado."
            if let dataURL = try? dataDirectoryURL(),
               let previousState = try? String(
                   contentsOf: dataURL.appendingPathComponent("zakuro-startup-state.txt"),
                   encoding: .utf8
               ),
               !previousState.isEmpty,
               previousState != "runtime.closed",
               previousState != "first-frame.success" {
                statusLabel.text = "Última fase del motor: \(previousState). Si el juego se cierra, envíame este estado."
            }
        }
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        let savedAt = UserDefaults.standard.double(forKey: "ykw2.controls.lastSavedAt")
        if savedAt > controlsSaveTimestampAtDisplay {
            statusLabel.text = "Controles táctiles guardados correctamente."
            controlsSaveTimestampAtDisplay = savedAt
        }
    }

    @objc private func openControlSettings() {
        let controller = ZakuroViewController(configurationOnly: true)
        navigationController?.pushViewController(controller, animated: true)
    }

    @objc private func configureCheats() {
        let alert = UIAlertController(title: "Cheats de rendimiento", message: "Selecciona un solo parche de FPS. La velocidad normal se puede combinar.", preferredStyle: .actionSheet)
        let fpsModes = [
            "FPS desactivados",
            "60 FPS (mantener R para 30 FPS)",
            "60 FPS (FPS++)",
        ]
        for (index, title) in fpsModes.enumerated() {
            alert.addAction(UIAlertAction(title: title, style: .default) { _ in
                UserDefaults.standard.set(index, forKey: "ykw2.cheats.fpsMode")
                self.refreshCheatsButtonTitle()
            })
        }
        let speedEnabled = UserDefaults.standard.bool(forKey: "ykw2.cheats.normalSpeed")
        alert.addAction(UIAlertAction(
            title: "Velocidad normal: \(speedEnabled ? "activada" : "desactivada") (alternar)",
            style: .default
        ) { _ in
            UserDefaults.standard.set(!speedEnabled, forKey: "ykw2.cheats.normalSpeed")
            self.refreshCheatsButtonTitle()
        })
        alert.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        if let popover = alert.popoverPresentationController {
            popover.sourceView = cheatsButton
            popover.sourceRect = cheatsButton.bounds
            popover.permittedArrowDirections = []
        }
        present(alert, animated: true)
    }

    private func refreshCheatsButtonTitle() {
        let mode = min(max(UserDefaults.standard.integer(forKey: "ykw2.cheats.fpsMode"), 0), 2)
        let fpsTitle = ["FPS desactivados", "60 FPS · R=30", "60 FPS++"][mode]
        let speedTitle = UserDefaults.standard.bool(forKey: "ykw2.cheats.normalSpeed") ? " · velocidad normal" : ""
        cheatsButton.setTitle("Cheats de rendimiento · \(fpsTitle)\(speedTitle)", for: .normal)
    }

    @objc private func openRuntime() {
        guard let romURL = try? romURL(),
              let dataURL = try? dataDirectoryURL(),
              FileManager.default.fileExists(atPath: romURL.path) else {
            statusLabel.text = "Selecciona primero tu copia autorizada de la ROM."
            return
        }
        do {
            try installConfiguredCheats(in: dataURL)
        } catch {
            statusLabel.text = "No se pudieron preparar los cheats: \(error.localizedDescription)"
            return
        }
        let controller = ZakuroViewController(romURL: romURL, dataURL: dataURL)
        if let navigationController {
            navigationController.pushViewController(controller, animated: true)
        } else {
            present(controller, animated: true)
        }
    }

    @objc private func selectRom() {
        guard rightsSwitch.isOn else {
            statusLabel.text = "Confirma primero que tienes derecho a usar la copia."
            return
        }
        let romType = UTType(filenameExtension: "3ds") ?? .data
        let picker = UIDocumentPickerViewController(
            // Some Files providers do not map .3ds to public.data. Including
            // public.item keeps the picker able to select the user's dump.
            forOpeningContentTypes: [romType, .data, .item],
            asCopy: true
        )
        picker.delegate = self
        picker.allowsMultipleSelection = false
        present(picker, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else {
            statusLabel.text = "No se seleccionó ningún archivo."
            return
        }
        let hasSecurityScope = url.startAccessingSecurityScopedResource()
        do {
            let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
            let size = (attributes[.size] as? NSNumber)?.uint64Value ?? 0
            guard size >= 0x200 else {
                if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
                statusLabel.text = "La imagen es demasiado pequeña."
                return
            }
            let handle = try FileHandle(forReadingFrom: url)
            defer { try? handle.close() }
            let header = try handle.read(upToCount: 0x200) ?? Data()
            guard header.count == 0x200,
                  header.subdata(in: 0x100..<0x104) == Data("NCSD".utf8) else {
                if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
                statusLabel.text = "La imagen no es un dump NCSD/3DS válido."
                return
            }
            try? handle.close()
            statusLabel.text = "NCSD válido. Copiando la ROM al almacenamiento privado del iPhone…"
            let destinationURL = try romURL()
            let directoryURL = try dataDirectoryURL()
            DispatchQueue.global(qos: .userInitiated).async {
                var copyError: Error?
                do {
                    try FileManager.default.createDirectory(
                        at: directoryURL,
                        withIntermediateDirectories: true
                    )
                    let temporaryURL = destinationURL.appendingPathExtension("part")
                    try? FileManager.default.removeItem(at: temporaryURL)
                    try FileManager.default.copyItem(at: url, to: temporaryURL)
                    if FileManager.default.fileExists(atPath: destinationURL.path) {
                        try FileManager.default.removeItem(at: destinationURL)
                    }
                    try FileManager.default.moveItem(at: temporaryURL, to: destinationURL)
                } catch {
                    copyError = error
                }
                DispatchQueue.main.async {
                    if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
                    if copyError == nil {
                        self.runtimeButton.isHidden = false
                        self.statusLabel.text = "ROM copiada al almacenamiento privado. Pulsa Iniciar para abrirla con Zakuro."
                    } else {
                        self.statusLabel.text = "No se pudo copiar la ROM: \(copyError!.localizedDescription)"
                    }
                }
            }
        } catch {
            if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
            statusLabel.text = "No se pudo leer la imagen: \(error.localizedDescription)"
        }
    }

    private func dataDirectoryURL() throws -> URL {
        try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        ).appendingPathComponent("ykw2-data", isDirectory: true)
    }

    private func romURL() throws -> URL {
        try dataDirectoryURL().appendingPathComponent("authorized-rom.3ds")
    }

    private func installConfiguredCheats(in dataURL: URL) throws {
        guard let bundledURL = Bundle.main.url(forResource: "00040000001B2900", withExtension: "txt") else {
            throw NSError(domain: "YKW2Cheats", code: 1, userInfo: [NSLocalizedDescriptionKey: "Falta el archivo de cheats integrado."])
        }
        let bundled = try String(contentsOf: bundledURL, encoding: .utf8)
        let cheatDirectory = dataURL.appendingPathComponent("cheats", isDirectory: true)
        try FileManager.default.createDirectory(at: cheatDirectory, withIntermediateDirectories: true)
        let targetURL = cheatDirectory.appendingPathComponent("00040000001B2900.txt")
        let existing = (try? String(contentsOf: targetURL, encoding: .utf8)) ?? ""
        let managedNames: Set<String> = [
            "60FPS (Hold R button for 30FPS)",
            "60FPS (FPS++)",
            "Game Speed Normal",
        ]

        var retainedLines: [String] = []
        var skipManagedSection = false
        for line in existing.components(separatedBy: .newlines) {
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            if trimmed.hasPrefix("["), trimmed.hasSuffix("]") {
                skipManagedSection = managedNames.contains(String(trimmed.dropFirst().dropLast()))
            }
            if !skipManagedSection { retainedLines.append(line) }
        }

        let mode = min(max(UserDefaults.standard.integer(forKey: "ykw2.cheats.fpsMode"), 0), 2)
        var enabledNames: Set<String> = []
        if mode == 1 { enabledNames.insert("60FPS (Hold R button for 30FPS)") }
        if mode == 2 { enabledNames.insert("60FPS (FPS++)") }
        if UserDefaults.standard.bool(forKey: "ykw2.cheats.normalSpeed") {
            enabledNames.insert("Game Speed Normal")
        }

        var configuredLines: [String] = []
        for line in bundled.components(separatedBy: .newlines) {
            configuredLines.append(line)
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            if trimmed.hasPrefix("["), trimmed.hasSuffix("]"),
               enabledNames.contains(String(trimmed.dropFirst().dropLast())) {
                configuredLines.append("*citra_enabled")
            }
        }

        let preservedText = retainedLines.joined(separator: "\n").trimmingCharacters(in: .newlines)
        let bundledText = configuredLines.joined(separator: "\n").trimmingCharacters(in: .newlines)
        let sections = [preservedText, bundledText].filter { !$0.isEmpty }
        try (sections.joined(separator: "\n\n") + "\n").write(to: targetURL, atomically: true, encoding: .utf8)
    }
}
